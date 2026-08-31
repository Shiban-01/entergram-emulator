#include "snr_vm.hpp"
#include <cstring>
#include <stdexcept>
#include <iostream>

namespace entergram {

// =============================================================================
// Entergram Switch SNR VM (Higurashi Hou / Umineko Saku format)
// Instruction format: [opcode_u8][args...] — variable-length.
// NumberStyle: VarInt. StringStyle: U8 (length-prefixed).
// =============================================================================

void SnrVm::load_script(const std::vector<uint8_t>& bytecode) {
    bytecode_ = bytecode;
    context_.current_address = 0;
    context_.frame_counter = 0;
    context_.running = true;
    context_.waiting = false;
    context_.wait_frames = 0;
}

void SnrVm::set_file_data(const std::vector<uint8_t>* file_data,
                          uint32_t bytecode_start, uint32_t st_offset) {
    file_data_ = file_data;
    bytecode_start_ = bytecode_start;
    string_table_offset_ = st_offset;
}

// ---- VarInt / read helpers ----

int64_t SnrVm::read_varint(const uint8_t* data, size_t& pos) {
    uint8_t t = data[pos];
    if ((t & 0x80) == 0) {
        pos += 1;
        int8_t v = static_cast<int8_t>(t);
        return v;
    }
    uint8_t p = (t & 0x70) >> 4;
    switch (p) {
        default:
        case 0: { // 12-bit signed constant
            uint8_t lo = data[pos + 1];
            int32_t val = ((t & 0x0F) << 8) | lo;
            if (val & 0x800) val |= ~0xFFF;
            pos += 2;
            return val;
        }
        case 1: { // 20-bit signed constant
            uint8_t b1 = data[pos + 1];
            uint8_t b2 = data[pos + 2];
            int32_t val = ((t & 0x0F) << 16) | (b2 << 8) | b1;
            if (val & 0x80000) val |= ~0xFFFFF;
            pos += 3;
            return val;
        }
        case 2: { // 28-bit signed constant
            uint8_t b1 = data[pos + 1];
            uint8_t b2 = data[pos + 2];
            uint8_t b3 = data[pos + 3];
            int32_t val = ((t & 0x0F) << 24) | (b3 << 16) | (b2 << 8) | b1;
            if (val & 0x8000000) val |= ~0xFFFFFFF;
            pos += 4;
            return val;
        }
        case 3: // 4-bit register
            pos += 1;
            return t & 0x0F;
        case 4: { // 12-bit register
            uint8_t lo = data[pos + 1];
            pos += 2;
            return ((t & 0x0F) << 8) | lo;
        }
        case 5: // 4-bit argument register
            pos += 1;
            return t & 0x0F;
        case 6: // literal 0x80000000
            pos += 1;
            return 0x80000000;
    }
}

uint8_t SnrVm::read_u8(const uint8_t* data, size_t& pos) { return data[pos++]; }
uint16_t SnrVm::read_u16(const uint8_t* data, size_t& pos) {
    uint16_t v; std::memcpy(&v, data + pos, 2); pos += 2; return v;
}
uint32_t SnrVm::read_u32(const uint8_t* data, size_t& pos) {
    uint32_t v; std::memcpy(&v, data + pos, 4); pos += 4; return v;
}

// Internal helpers
static int64_t read_number(const uint8_t* d, size_t& p) { return SnrVm::read_varint(d, p); }
static int64_t read_reg(const uint8_t* d, size_t& p) { return SnrVm::read_varint(d, p); }
static uint32_t read_offset(const uint8_t* d, size_t& p) { return SnrVm::read_u32(d, p); }
static uint8_t read_u8(const uint8_t* d, size_t& p) { return SnrVm::read_u8(d, p); }
static uint16_t read_u16(const uint8_t* d, size_t& p) { return SnrVm::read_u16(d, p); }
static uint32_t read_u32(const uint8_t* d, size_t& p) { return SnrVm::read_u32(d, p); }

static std::string read_string_u8(const uint8_t* data, size_t& pos) {
    uint8_t len = data[pos++];
    std::string s(reinterpret_cast<const char*>(data + pos), len);
    pos += len;
    if (data[pos] == 0) pos++;
    return s;
}

static std::vector<std::string> read_string_array(const uint8_t* data, size_t& pos) {
    uint8_t count = data[pos++];
    std::vector<std::string> result;
    for (uint8_t i = 0; i < count; i++) {
        result.push_back(read_string_u8(data, pos));
    }
    return result;
}

static std::vector<int64_t> read_number_array(const uint8_t* data, size_t& pos) {
    uint8_t count = data[pos++];
    std::vector<int64_t> result;
    for (uint8_t i = 0; i < count; i++) {
        result.push_back(read_number(data, pos));
    }
    return result;
}

static std::vector<int64_t> read_reg_array(const uint8_t* data, size_t& pos) {
    uint8_t count = data[pos++];
    std::vector<int64_t> result;
    for (uint8_t i = 0; i < count; i++) {
        result.push_back(read_reg(data, pos));
    }
    return result;
}

// bitmask_u8_number_array: read u8 mask, then one number per set bit
static std::vector<int64_t> read_bitmask_u8(const uint8_t* data, size_t& pos) {
    uint8_t mask = data[pos++];
    std::vector<int64_t> result;
    for (int b = 0; b < 8; b++) {
        if (mask & (1 << b)) {
            result.push_back(read_number(data, pos));
        }
    }
    return result;
}

// ---- String resolution ----

std::string SnrVm::get_string(uint32_t offset) const {
    if (offset == 0) return "";
    if (!file_data_ || offset >= file_data_->size()) return "";
    const char* start = reinterpret_cast<const char*>(&(*file_data_)[offset]);
    size_t max_len = file_data_->size() - offset;
    size_t len = strnlen(start, max_len);
    return std::string(start, len);
}

std::string SnrVm::get_string_rel(uint32_t offset) const {
    if (!file_data_) return "";
    uint32_t abs_offset = bytecode_start_ + offset;
    if (abs_offset >= file_data_->size()) return "";
    const char* start = reinterpret_cast<const char*>(&(*file_data_)[abs_offset]);
    size_t max_len = file_data_->size() - abs_offset;
    size_t len = strnlen(start, max_len);
    return std::string(start, len);
}

// ---- VM execution ----

bool SnrVm::step(SnrVmCallbacks* callbacks) {
    if (!context_.running || bytecode_.empty()) {
        return false;
    }

    // Handle wait countdown
    if (context_.waiting) {
        if (context_.wait_frames > 0) {
            context_.wait_frames--;
            if (context_.wait_frames == 0) {
                context_.waiting = false;
            }
            return false; // Yield during wait
        }
        context_.waiting = false;
    }

    size_t pos = context_.current_address;
    if (pos >= bytecode_.size()) {
        context_.running = false;
        return false;
    }

    uint8_t opcode = bytecode_[pos];
    pos++;

    bool is_command = (opcode != 0 && opcode >= 0x80) || opcode == 0;

    if (opcode == 0) {
        // EXIT command
        read_number(bytecode_.data(), pos);
        context_.running = false;
        if (callbacks) callbacks->on_exit();
        return false;
    }

    if (!is_command) {
        // ---- Instructions (0x40-0x53, < 0x80, nonzero) ----
        switch (opcode) {
            case 0x46: { // JC (conditional jump)
                uint8_t cond = read_u8(bytecode_.data(), pos);
                int64_t n1 = read_number(bytecode_.data(), pos);
                int64_t n2 = read_number(bytecode_.data(), pos);
                uint32_t target = read_offset(bytecode_.data(), pos);
                bool do_jump = false;
                switch (cond) {
                    case 0x00: do_jump = (n1 != 0); break;      // truthy
                    case 0x01: do_jump = (n1 == 0); break;      // falsy
                    case 0x02: do_jump = (n1 == n2); break;     // equal
                    case 0x03: do_jump = (n1 != n2); break;     // not equal
                    case 0x04: do_jump = (n1 > n2); break;      // greater
                    case 0x05: do_jump = (n1 >= n2); break;     // greater-or-equal
                    case 0x06: do_jump = (n1 < n2); break;      // less
                    case 0x07: do_jump = (n1 <= n2); break;     // less-or-equal
                    default:   do_jump = (n1 != 0); break;
                }
                if (do_jump) context_.current_address = target;
                else context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            case 0x47: { // JMP
                uint32_t target = read_offset(bytecode_.data(), pos);
                context_.current_address = target;
                return true;
            }
            case 0x48: { // GOSUB
                uint32_t target = read_offset(bytecode_.data(), pos);
                context_.current_address = target;
                return true;
            }
            case 0x49: { // RETSUB
                pos++; // no args
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            case 0x4d: { // PUSH
                read_number_array(bytecode_.data(), pos);
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            case 0x4e: { // POP
                read_reg_array(bytecode_.data(), pos);
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            case 0x4f: { // CALL
                uint32_t target = read_offset(bytecode_.data(), pos);
                read_number_array(bytecode_.data(), pos);
                context_.current_address = target;
                return true;
            }
            case 0x50: { // RETURN
                pos++; // no args
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            case 0x41: { // BO (binary op)
                uint8_t op = read_u8(bytecode_.data(), pos);
                read_reg(bytecode_.data(), pos);
                read_number(bytecode_.data(), pos);  // optional number
                read_number(bytecode_.data(), pos);
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            case 0x40: { // UO (unary op)
                read_u8(bytecode_.data(), pos);
                read_reg(bytecode_.data(), pos);
                read_number(bytecode_.data(), pos);  // optional number
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            case 0x42: { // EXP
                read_reg(bytecode_.data(), pos);
                // Expression encoding is complex (RPN via instructions) — skip heuristically
                pos += 1;
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            case 0x4a: { // JT
                read_number(bytecode_.data(), pos);
                {
                    uint8_t cnt = read_u8(bytecode_.data(), pos);
                    for (uint8_t i = 0; i < cnt; i++) read_offset(bytecode_.data(), pos);
                }
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            case 0x4b: { // GOSUBT
                read_number(bytecode_.data(), pos);
                {
                    uint8_t cnt = read_u8(bytecode_.data(), pos);
                    for (uint8_t i = 0; i < cnt; i++) read_offset(bytecode_.data(), pos);
                }
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            case 0x4c: { // RND
                read_reg(bytecode_.data(), pos);
                read_number(bytecode_.data(), pos);
                read_number(bytecode_.data(), pos);
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            case 0x43: { // MM
                read_number(bytecode_.data(), pos);
                read_reg_array(bytecode_.data(), pos);
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            case 0x44: { // GT
                read_reg(bytecode_.data(), pos);
                read_number(bytecode_.data(), pos);
                pos += 1; // length byte
                {
                    uint8_t cnt = pos < bytecode_.size() ? bytecode_[pos] : 0;
                    pos++;
                    for (uint8_t i = 0; i < cnt; i++) read_number(bytecode_.data(), pos);
                }
                break;
            }
            case 0x45: { // ST
                read_number(bytecode_.data(), pos);
                read_number(bytecode_.data(), pos);
                pos += 1;
                {
                    uint8_t cnt = pos < bytecode_.size() ? bytecode_[pos] : 0;
                    pos++;
                    for (uint8_t i = 0; i < cnt; i++) read_number(bytecode_.data(), pos);
                }
                break;
            }
            case 0x51: { // FMT
                read_reg(bytecode_.data(), pos);
                read_number(bytecode_.data(), pos);
                read_number(bytecode_.data(), pos);
                pos += 1;
                {
                    uint16_t cnt = read_u16(bytecode_.data(), pos);
                    for (uint16_t i = 0; i < cnt; i++) read_number(bytecode_.data(), pos);
                }
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            default: {
                pos += 1;
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
        }
    } else {
        // ---- Commands (opcode >= 0x80, or == 0x00 = EXIT) ----
        // (EXIT handled above)
        switch (opcode) {
            case 0x82: { // SSET
                read_number(bytecode_.data(), pos);
                read_number(bytecode_.data(), pos);
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            case 0x83: { // WAIT
                int64_t frames = read_number(bytecode_.data(), pos);
                if (callbacks) callbacks->on_wait(static_cast<int>(frames));
                context_.current_address = static_cast<uint32_t>(pos);
                context_.frame_counter++;
                return false; // Yield
            }
            case 0x84: { // KEYWAIT
                int64_t key_id = read_number(bytecode_.data(), pos);
                if (callbacks) callbacks->on_keywait(static_cast<int>(key_id));
                context_.current_address = static_cast<uint32_t>(pos);
                return false; // Yield
            }
            case 0x85: { // MSGINIT (HigurashiHou: number + number + number)
                read_number(bytecode_.data(), pos);
                read_number(bytecode_.data(), pos);
                read_number(bytecode_.data(), pos);
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            case 0x86: { // MSGSET (u32 msgid + string U8)
                read_u32(bytecode_.data(), pos);
                std::string text = read_string_u8(bytecode_.data(), pos);
                if (callbacks) callbacks->on_text_display(text, "");
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            case 0x88: { // MSGSIGNAL
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            case 0x8a: { // MSGCLOSE
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            case 0x8c: { // LOGSET
                read_string_u8(bytecode_.data(), pos);
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            case 0x8d: { // SELECT
                read_u16(bytecode_.data(), pos);  // u16
                read_u16(bytecode_.data(), pos);  // u16
                read_reg(bytecode_.data(), pos);   // reg
                read_number(bytecode_.data(), pos); // number
                std::string caption = read_string_u8(bytecode_.data(), pos);
                std::vector<std::string> options = read_string_array(bytecode_.data(), pos);
                if (callbacks) callbacks->on_choice(options);
                context_.current_address = static_cast<uint32_t>(pos);
                return false; // Yield — waiting for choice
            }
            case 0x8e: { // WIPE (HigurashiSui: HiguSuiWipeArg)
                read_bitmask_u8(bytecode_.data(), pos);
                // The format is: u8 b1, u8 b2, then numbers per bitmask
                // For HigurashiHou, WIPE takes number+number+number+bitmask_u8
                read_number(bytecode_.data(), pos);
                read_number(bytecode_.data(), pos);
                read_number(bytecode_.data(), pos);
                read_bitmask_u8(bytecode_.data(), pos);
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            case 0x8f: { // WIPEWAIT
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            case 0x90: { // BGMPLAY (number×4)
                read_number(bytecode_.data(), pos);
                read_number(bytecode_.data(), pos);
                read_number(bytecode_.data(), pos);
                read_number(bytecode_.data(), pos);
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            case 0x91: { // BGMSTOP
                read_number(bytecode_.data(), pos);
                if (callbacks) callbacks->on_bgm_stop();
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            case 0x95: { // SEPLAY (5 numbers for HigurashiSui)
                read_number(bytecode_.data(), pos);
                read_number(bytecode_.data(), pos);
                read_number(bytecode_.data(), pos);
                read_number(bytecode_.data(), pos);
                read_number(bytecode_.data(), pos);
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            case 0x96: { // SESTOP
                read_number(bytecode_.data(), pos);
                read_number(bytecode_.data(), pos);
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            case 0x97: { // SESTOPALL
                read_number(bytecode_.data(), pos);
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            case 0x98: { // SEVOL
                read_number(bytecode_.data(), pos);
                read_number(bytecode_.data(), pos);
                read_number(bytecode_.data(), pos);
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            case 0x9a: { // SEWAIT
                read_number(bytecode_.data(), pos);
                read_number(bytecode_.data(), pos);
                context_.current_address = static_cast<uint32_t>(pos);
                return false; // Yield
            }
            case 0x9b: { // SEONCE (3 numbers for HigurashiSui)
                read_number(bytecode_.data(), pos);
                read_number(bytecode_.data(), pos);
                read_number(bytecode_.data(), pos);
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            case 0x9c: { // VOICEPLAY (string U8 + number + number)
                std::string voice_file = read_string_u8(bytecode_.data(), pos);
                int64_t n1 = read_number(bytecode_.data(), pos);
                int64_t n2 = read_number(bytecode_.data(), pos);
                if (callbacks) callbacks->on_voice_play(voice_file, static_cast<int>(n1), static_cast<int>(n2));
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            case 0x9d: { // VOICESTOP
                read_number(bytecode_.data(), pos);
                if (callbacks) callbacks->on_voice_stop(0);
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            case 0x9e: { // VOICEWAIT
                read_number(bytecode_.data(), pos);
                context_.current_address = static_cast<uint32_t>(pos);
                return false; // Yield
            }
            case 0xa1: { // AUTOSAVE
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            case 0xa2: { // EVBEGIN
                read_number(bytecode_.data(), pos);
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            case 0xa3: { // EVEND
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            case 0xa0: { // SAVEINFO
                read_number(bytecode_.data(), pos);
                read_string_u8(bytecode_.data(), pos);
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            case 0xc0: { // LAYERINIT (number + bitmask_u8_number_array)
                read_number(bytecode_.data(), pos);
                read_bitmask_u8(bytecode_.data(), pos);
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            case 0xc1: { // LAYERLOAD (number + number + number + bitmask_u8_number_array)
                int64_t n1 = read_number(bytecode_.data(), pos);
                int64_t n2 = read_number(bytecode_.data(), pos);
                int64_t n3 = read_number(bytecode_.data(), pos);
                read_bitmask_u8(bytecode_.data(), pos);
                if (callbacks) callbacks->on_layer_load(static_cast<int>(n1), static_cast<int>(n2), static_cast<int>(n3), 0);
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            case 0xc2: { // LAYERUNLOAD
                read_number(bytecode_.data(), pos);
                read_number(bytecode_.data(), pos);
                if (callbacks) callbacks->on_layer_unload(0);
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            case 0xc3: { // LAYERCTRL (number + number + bitmask_u8_number_array)
                int64_t n1 = read_number(bytecode_.data(), pos);
                int64_t n2 = read_number(bytecode_.data(), pos);
                std::vector<int64_t> args = read_bitmask_u8(bytecode_.data(), pos);
                if (callbacks && args.size() >= 2) {
                    callbacks->on_layer_ctrl(static_cast<int>(n1), static_cast<int>(n2),
                        static_cast<int>(args[0]), static_cast<int>(args[1]), 0, 0);
                }
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            case 0xc4: { // LAYERWAIT
                read_number(bytecode_.data(), pos);
                // number_array U8
                {
                    uint8_t cnt = read_u8(bytecode_.data(), pos);
                    for (uint8_t i = 0; i < cnt; i++) read_number(bytecode_.data(), pos);
                }
                context_.current_address = static_cast<uint32_t>(pos);
                return false; // Yield
            }
            case 0xc6: { // LAYERSWAP
                read_number(bytecode_.data(), pos);
                read_number(bytecode_.data(), pos);
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            case 0xc7: { // LAYERSELECT
                read_number(bytecode_.data(), pos);
                read_number(bytecode_.data(), pos);
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            case 0xc8: { // MOVIEWAIT
                read_number(bytecode_.data(), pos);
                read_number(bytecode_.data(), pos);
                context_.current_address = static_cast<uint32_t>(pos);
                return false; // Yield
            }
            case 0xd0: { // TIPSGET
                {
                    uint8_t cnt = read_u8(bytecode_.data(), pos);
                    for (uint8_t i = 0; i < cnt; i++) read_number(bytecode_.data(), pos);
                }
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            case 0xd4: { // SNRSEL
                read_number(bytecode_.data(), pos);
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            case 0xff: { // DEBUGOUT
                read_string_u8(bytecode_.data(), pos);
                pos += 1; // u8
                // number_array u8
                {
                    uint8_t cnt = read_u8(bytecode_.data(), pos);
                    for (uint8_t i = 0; i < cnt; i++) read_number(bytecode_.data(), pos);
                }
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
            default: {
                // Unknown command — skip one varint to avoid infinite loop
                read_number(bytecode_.data(), pos);
                context_.current_address = static_cast<uint32_t>(pos);
                return true;
            }
        }
    }

    // Fallback
    context_.current_address = static_cast<uint32_t>(pos);
    return true;
}

void SnrVm::run(SnrVmCallbacks* callbacks) {
    while (context_.running) {
        if (!step(callbacks)) {
            if (!context_.waiting) break;
        }
        context_.frame_counter++;
    }
}

} // namespace entergram
