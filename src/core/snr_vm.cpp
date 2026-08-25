#include "snr_vm.hpp"
#include <cstring>
#include <stdexcept>

namespace entergram {

// =============================================================================
// SNR0 Instruction Format:
//   18-byte instructions executed sequentially from the loaded bytecode.
//   String references use arg1/arg2 as offsets into a string table at the
//   end of the bytecode.
// =============================================================================

void SnrVm::load_script(const std::vector<uint8_t>& bytecode) {
    bytecode_ = bytecode;
    context_.current_address = 0;
    context_.frame_counter = 0;
    context_.running = true;
}

uint32_t SnrVm::read_u32(uint32_t offset) const {
    if (offset + 4 > bytecode_.size()) {
        return 0;
    }
    uint32_t value;
    std::memcpy(&value, &bytecode_[offset], 4);
    return value;
}

SnrInstruction SnrVm::decode_instruction(uint32_t offset) const {
    SnrInstruction instr{};
    if (offset + sizeof(SnrInstruction) > bytecode_.size()) {
        instr.opcode = static_cast<uint8_t>(Opcode::NOP);
        return instr;
    }

    instr.flags = bytecode_[offset];
    instr.opcode = bytecode_[offset + 1];
    instr.arg1 = read_u32(offset + 2);
    instr.arg2 = read_u32(offset + 6);
    instr.arg3 = read_u32(offset + 10);
    instr.arg4 = read_u32(offset + 14);

    return instr;
}

std::string SnrVm::get_string(uint32_t offset) const {
    if (offset >= bytecode_.size()) {
        return "";
    }

    const char* start = reinterpret_cast<const char*>(&bytecode_[offset]);
    size_t max_len = bytecode_.size() - offset;
    size_t len = strnlen(start, max_len);

    return std::string(start, len);
}

bool SnrVm::step(SnrVmCallbacks* callbacks) {
    if (!context_.running || bytecode_.empty()) {
        return false;
    }

    if (context_.current_address + sizeof(SnrInstruction) > bytecode_.size()) {
        context_.running = false;
        return false;
    }

    SnrInstruction instr = decode_instruction(context_.current_address);
    return execute(instr, callbacks);
}

void SnrVm::run(SnrVmCallbacks* callbacks) {
    while (context_.running) {
        if (!step(callbacks)) {
            break;
        }
        context_.frame_counter++;
    }
}

bool SnrVm::execute(const SnrInstruction& instr, SnrVmCallbacks* callbacks) {
    auto opcode = static_cast<Opcode>(instr.opcode);
    uint32_t next_addr = context_.current_address + sizeof(SnrInstruction);

    switch (opcode) {
        case Opcode::WAIT:
            if (callbacks) callbacks->on_wait(instr.arg1);
            context_.current_address = next_addr;
            return false;  // Yield

        case Opcode::VOICEPLAY:
            if (callbacks) {
                callbacks->on_voice_play(
                    get_string(instr.arg1),
                    static_cast<int>(instr.arg2),
                    static_cast<int>(instr.arg3)
                );
            }
            context_.current_address = next_addr;
            return true;

        case Opcode::VOICESTOP:
            if (callbacks) callbacks->on_voice_stop(static_cast<int>(instr.arg1));
            context_.current_address = next_addr;
            return true;

        case Opcode::VOICEWAIT:
            if (callbacks) callbacks->on_voice_wait(static_cast<int>(instr.arg1));
            context_.current_address = next_addr;
            return true;

        case Opcode::BGM_PLAY:
            if (callbacks) {
                callbacks->on_bgm_play(get_string(instr.arg1), static_cast<int>(instr.arg2));
            }
            context_.current_address = next_addr;
            return true;

        case Opcode::BGM_STOP:
            if (callbacks) callbacks->on_bgm_stop();
            context_.current_address = next_addr;
            return true;

        case Opcode::SE_PLAY:
            if (callbacks) {
                callbacks->on_se_play(get_string(instr.arg1), static_cast<int>(instr.arg2));
            }
            context_.current_address = next_addr;
            return true;

        case Opcode::MOVIE:
            if (callbacks) callbacks->on_movie_play(get_string(instr.arg1));
            context_.current_address = next_addr;
            return true;

        case Opcode::TEXT:
            if (callbacks) {
                callbacks->on_text_display(get_string(instr.arg1), get_string(instr.arg2));
            }
            context_.current_address = next_addr;
            return true;

        case Opcode::NAME:
            if (callbacks) {
                callbacks->on_text_display("", get_string(instr.arg1));
            }
            context_.current_address = next_addr;
            return true;

        case Opcode::CALL:
            context_.current_address = instr.arg1;
            return true;

        case Opcode::RET:
            context_.current_address = next_addr;
            return true;

        case Opcode::JUMP:
            context_.current_address = instr.arg1;
            return true;

        case Opcode::SYS:
            if (callbacks) {
                callbacks->on_system_call(instr.arg1, get_string(instr.arg2));
            }
            context_.current_address = next_addr;
            return true;

        case Opcode::NOP:
            context_.current_address = next_addr;
            return true;

        default:
            context_.current_address = next_addr;
            return true;
    }
}

} // namespace entergram
