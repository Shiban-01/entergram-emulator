#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace entergram {

// SNR0 instruction format (18 bytes):
//   byte 0:    flags (u8)    — execution flags (e.g. LINK, CALL)
//   byte 1:    opcode  (u8)   — command type
//   bytes 2-5:  arg1  (u32 LE) — first argument
//   bytes 6-9:  arg2  (u32 LE) — second argument
//   bytes 10-13: arg3 (u32 LE) — third argument
//   bytes 14-17: arg4 (u32 LE) — fourth argument
//
// Some opcodes use arg1/arg2 as offsets into a string table.

#pragma pack(push, 1)
struct SnrInstruction {
    uint8_t flags;
    uint8_t opcode;
    uint32_t arg1;
    uint32_t arg2;
    uint32_t arg3;
    uint32_t arg4;
};
static_assert(sizeof(SnrInstruction) == 18, "SNR instruction must be 18 bytes");
#pragma pack(pop)

// Opcode definitions (reverse-engineered from Umineko SNR0 format)
enum class Opcode : uint8_t {
    SYS    = 0x00,
    WAIT   = 0x01,
    CALL   = 0x02,
    RET    = 0x03,
    JUMP   = 0x04,
    TEXT   = 0x10,
    NAME   = 0x11,
    CHOICE = 0x12,
    VOICEPLAY = 0x9c,
    VOICESTOP = 0x9d,
    VOICEWAIT = 0x9e,
    BGM_PLAY  = 0xa0,
    BGM_STOP  = 0xa1,
    SE_PLAY   = 0xa2,
    MOVIE     = 0xb0,
    LOAD      = 0xc0,
    LAYER     = 0xc1,
    ALPHA     = 0xc2,
    MOVE      = 0xc3,
    NOP       = 0xff,
};

struct VmContext {
    uint32_t current_address = 0;
    uint32_t frame_counter = 0;
    bool running = true;
};

class SnrVmCallbacks {
public:
    virtual ~SnrVmCallbacks() = default;
    virtual void on_voice_play(const std::string& file_name, int volume, int flags) {}
    virtual void on_voice_stop(int voice_id) {}
    virtual void on_voice_wait(int voice_id) {}
    virtual void on_bgm_play(const std::string& file_name, int volume) {}
    virtual void on_bgm_stop() {}
    virtual void on_se_play(const std::string& file_name, int volume) {}
    virtual void on_movie_play(const std::string& file_name) {}
    virtual void on_movie_stop() {}
    virtual void on_text_display(const std::string& text, const std::string& character_name) {}
    virtual void on_choice(const std::vector<std::string>& options) {}
    virtual void on_system_call(uint32_t code, const std::string& data) {}
    virtual void on_wait(int frames) {}
};

class SnrVm {
public:
    SnrVm() = default;
    ~SnrVm() = default;

    void load_script(const std::vector<uint8_t>& bytecode);
    bool step(SnrVmCallbacks* callbacks);
    void run(SnrVmCallbacks* callbacks);

    std::string get_string(uint32_t offset) const;

    uint32_t current_address() const { return context_.current_address; }
    bool is_running() const { return context_.running; }
    size_t script_size() const { return bytecode_.size(); }
    const VmContext& context() const { return context_; }

    // Decode/read helpers (public for testing)
    SnrInstruction decode_instruction(uint32_t offset) const;
    uint32_t read_u32(uint32_t offset) const;

private:
    std::vector<uint8_t> bytecode_;
    VmContext context_;

    bool execute(const SnrInstruction& instr, SnrVmCallbacks* callbacks);
};

} // namespace entergram
