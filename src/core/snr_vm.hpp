#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <optional>
#include <cstddef>

namespace entergram {

// =============================================================================
// Entergram Switch SNR VM (Higurashi Hou / Umineko Saku format)
//
// Instruction format: [opcode_u8][args...] — variable-length, no size prefix.
// Numbers use VarInt encoding (NumberStyle::VarInt for Switch versions).
// Strings use U8-length-prefixed format: [u8 len][bytes][optional null].
//
// Opcode mapping (from masagrator's get_opcode_name for HigurashiHou):
//   < 0x80 = Instructions (0x40-0x53): uo, bo, exp, mm, gt, st, jc, j, gosub,
//             retsub, jt, gosubt, rnd, push, pop, call, return, fmt, fnmt, getbupid
//   0x00 + >= 0x80 = Commands (>= 0x80, with 0x00 being EXIT)
//
// VarInt encoding (NumberStyle::VarInt):
//   If t & 0x80 == 0: 7-bit signed constant
//   If t & 0x80 != 0:
//     P = (t & 0x70) >> 4
//     P==0: 12-bit signed (t & 0x0F) + 1 byte
//     P==1: 20-bit signed (t & 0x0F) + 2 bytes
//     P==2: 28-bit signed (t & 0x0F) + 3 bytes
//     P==3: 4-bit register
//     P==4: 12-bit register (t & 0x0F) + 1 byte
//     P==5: 4-bit argument register
//     P==6: literal 0x80000000
//
// NOTE: In HigurashiHou, EXIT has opcode 0x00 (when opcode != 0 check is
// applied, 0x00 is treated specially). Other commands are >= 0x80.
// =============================================================================

// Instructions (opcodes 0x40-0x53, all < 0x80)
enum class InstrOpcode : uint8_t {
    UO      = 0x40,
    BO      = 0x41,
    EXP     = 0x42,
    MM      = 0x43,
    GT      = 0x44,
    ST      = 0x45,
    JC      = 0x46,  // Jump if condition
    JMP     = 0x47,  // Unconditional jump
    GOSUB   = 0x48,  // Call subroutine
    RETSUB  = 0x49,  // Return from subroutine
    JT      = 0x4a,  // Jump table
    GOSUBT  = 0x4b,  // Call subroutine table
    RND     = 0x4c,  // Random
    PUSH    = 0x4d,  // Push number array
    POP     = 0x4e,  // Pop register array
    CALL    = 0x4f,  // Call function
    RETURN  = 0x50,  // Return from function
};

// Commands (opcodes >= 0x80, plus 0x00 for EXIT in HigurashiHou)
enum class CmdOpcode : uint8_t {
    EXIT       = 0x00,  // Special: EXIT is 0x00 in HigurashiHou
    SGET       = 0x81,
    SSET       = 0x82,
    WAIT       = 0x83,
    KEYWAIT    = 0x84,
    MSGINIT    = 0x85,
    MSGSET     = 0x86,
    MSGWAIT    = 0x87,
    MSGSIGNAL  = 0x88,
    MSGSYNC    = 0x89,
    MSGCLOSE   = 0x8a,
    MSGFACE    = 0x8b,
    LOGSET     = 0x8c,
    SELECT     = 0x8d,
    WIPE       = 0x8e,
    WIPEWAIT   = 0x8f,
    BGMPLAY    = 0x90,
    BGMSTOP    = 0x91,
    BGMVOL     = 0x92,
    BGMWAIT    = 0x93,
    BGMSYNC    = 0x94,
    SEPLAY     = 0x95,
    SESTOP     = 0x96,
    SESTOPALL  = 0x97,
    SEVOL      = 0x98,
    SEPAN      = 0x99,
    SEWAIT     = 0x9a,
    SEONCE     = 0x9b,
    VOICEPLAY  = 0x9c,
    VOICESTOP  = 0x9d,
    VOICEWAIT  = 0x9e,
    SAVEINFO   = 0xa0,
    AUTOSAVE   = 0xa1,
    EVBEGIN    = 0xa2,
    EVEND      = 0xa3,
    TROPHY     = 0xb0,
    LAYERINIT  = 0xc0,
    LAYERLOAD  = 0xc1,
    LAYERUNLOAD = 0xc2,
    LAYERCTRL  = 0xc3,
    LAYERWAIT  = 0xc4,
    LAYERBACK  = 0xc5,
    LAYERSWAP  = 0xc6,
    LAYERSELECT = 0xc7,
    MOVIEWAIT  = 0xc8,
    FEELICON   = 0xc9,
    TIPSGET    = 0xd0,
    CHARSELECT = 0xd1,
    OTSUGET    = 0xd2,
    CHART      = 0xd3,
    SNRSEL     = 0xd4,
    KAKERA     = 0xd5,
    KAKERAGET  = 0xd6,
    QUIZ       = 0xd7,
    FAKESELECT = 0xd8,
    UNLOCK     = 0xd9,
    DEBUGOUT   = 0xff,
};

struct VmContext {
    uint32_t current_address = 0;
    uint32_t frame_counter = 0;
    bool running = true;
    bool waiting = false;
    uint32_t wait_frames = 0;
    uint32_t current_address_at_wait = 0;
};

class SnrVmCallbacks {
public:
    virtual ~SnrVmCallbacks() = default;
    // Voice
    virtual void on_voice_play(const std::string& file_name, int volume, int flags) {}
    virtual void on_voice_stop(int voice_id) {}
    virtual void on_voice_wait(int voice_id) {}
    // BGM
    virtual void on_bgm_play(const std::string& file_name, int volume) {}
    virtual void on_bgm_stop() {}
    // SE
    virtual void on_se_play(const std::string& file_name, int volume) {}
    virtual void on_se_stop(int se_id) {}
    // Movie
    virtual void on_movie_play(const std::string& file_name) {}
    virtual void on_movie_stop() {}
    // Text
    virtual void on_text_display(const std::string& text, const std::string& character_name) {}
    virtual void on_message_signal(int signal) {}
    virtual void on_message_close(int unk) {}
    virtual void on_wait(int frames) {}
    virtual void on_keywait(int key_id) {}
    virtual void on_choice(const std::vector<std::string>& options) {}
    virtual void on_system_call(uint32_t code, const std::string& data) {}
    // Layers/sprites
    virtual void on_layer_load(int layer_id, int layer_type, int param1, int param2) {}
    virtual void on_layer_ctrl(int layer_id, int property_id, int target, int duration, int flags, int easing) {}
    virtual void on_layer_unload(int layer_id) {}
    virtual void on_sprite_move(int layer_id, int x, int y, int duration, int flags) {}
    virtual void on_sprite_alpha(int layer_id, int alpha, int duration, int flags) {}
    virtual void on_wipe(int type, int duration, int flags, int easing) {}
    virtual void on_screen_effect(int effect, int duration, int flags, int easing) {}
    // Select
    virtual void on_select(int a, int b, int reg, int num, const std::vector<std::string>& options) {}
    // System
    virtual void on_exit() {}
    // Save/Load
    virtual void on_save_info(int slot, const std::string& name) {}
    virtual void on_autosave() {}
};

class SnrVm {
public:
    SnrVm() = default;
    ~SnrVm() = default;

    void load_script(const std::vector<uint8_t>& bytecode);
    void set_file_data(const std::vector<uint8_t>* file_data, uint32_t bytecode_start, uint32_t st_offset);

    bool step(SnrVmCallbacks* callbacks);
    void run(SnrVmCallbacks* callbacks);

    std::string get_string(uint32_t offset) const;
    std::string get_string_rel(uint32_t offset) const;

    uint32_t current_address() const { return context_.current_address; }
    bool is_running() const { return context_.running; }
    bool is_waiting() const { return context_.waiting; }
    int wait_frames_remaining() const { return context_.wait_frames; }
    size_t script_size() const { return bytecode_.size(); }
    const VmContext& context() const { return context_; }

    // Static decode helpers (public for testing/debugging)
    static int64_t read_varint(const uint8_t* data, size_t& pos);
    static uint8_t read_u8(const uint8_t* data, size_t& pos);
    static uint16_t read_u16(const uint8_t* data, size_t& pos);
    static uint32_t read_u32(const uint8_t* data, size_t& pos);

private:
    std::vector<uint8_t> bytecode_;
    const std::vector<uint8_t>* file_data_ = nullptr;
    uint32_t bytecode_start_ = 0;
    uint32_t string_table_offset_ = 0;
    VmContext context_;
};

} // namespace entergram
