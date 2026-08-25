#include "../src/core/snr_vm.hpp"
#include <iostream>
#include <cassert>
#include <cstdio>

using namespace entergram;

class TestCallbacks : public SnrVmCallbacks {
public:
    int voice_play_count = 0;
    int voice_stop_count = 0;
    int movie_play_count = 0;
    int text_count = 0;
    int wait_count = 0;
    std::string last_voice_file;
    std::string last_movie_file;
    std::string last_text;
    int last_wait_frames = 0;

    void on_voice_play(const std::string& file_name, int volume, int flags) override {
        voice_play_count++;
        last_voice_file = file_name;
    }

    void on_voice_stop(int voice_id) override {
        voice_stop_count++;
    }

    void on_movie_play(const std::string& file_name) override {
        movie_play_count++;
        last_movie_file = file_name;
    }

    void on_text_display(const std::string& text, const std::string& character_name) override {
        text_count++;
        last_text = text;
    }

    void on_wait(int frames) override {
        wait_count++;
        last_wait_frames = frames;
    }
};

void test_instruction_size() {
    static_assert(sizeof(SnrInstruction) == 18, "SNR instruction must be 18 bytes");
    printf("PASS: test_instruction_size\n");
}

void test_decode_instruction() {
    uint8_t bytes[18] = {
        0x01,                   // flags
        0x01,                   // opcode = WAIT
        0x1e, 0x00, 0x00, 0x00, // arg1 = 30
        0x00, 0x00, 0x00, 0x00, // arg2 = 0
        0x00, 0x00, 0x00, 0x00, // arg3 = 0
        0x00, 0x00, 0x00, 0x00  // arg4 = 0
    };

    SnrVm vm;
    vm.load_script(std::vector<uint8_t>(bytes, bytes + 18));

    auto instr = vm.decode_instruction(0);
    assert(instr.flags == 0x01);
    assert(instr.opcode == 0x01);  // WAIT
    assert(instr.arg1 == 30);

    printf("PASS: test_decode_instruction\n");
}

void test_wait_instruction() {
    // WAIT returns false (yields), advances address
    uint8_t bytecode[18] = {
        0x00, 0x01,             // flags=0, opcode=WAIT
        0x1e, 0x00, 0x00, 0x00, // arg1 = 30
        0x00, 0x00, 0x00, 0x00, // arg2
        0x00, 0x00, 0x00, 0x00, // arg3
        0x00, 0x00, 0x00, 0x00  // arg4
    };

    SnrVm vm;
    vm.load_script(std::vector<uint8_t>(bytecode, bytecode + 18));

    TestCallbacks callbacks;
    bool yielded = vm.step(&callbacks);
    assert(!yielded);  // WAIT yields — step returns false
    assert(vm.current_address() == 18);
    assert(callbacks.wait_count == 1);
    assert(callbacks.last_wait_frames == 30);

    printf("PASS: test_wait_instruction\n");
}

void test_nop_instruction() {
    uint8_t bytecode[18] = {
        0x00, 0xff,  // NOP
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };

    SnrVm vm;
    vm.load_script(std::vector<uint8_t>(bytecode, bytecode + 18));

    TestCallbacks callbacks;
    bool cont = vm.step(&callbacks);
    assert(cont);  // NOP continues — step returns true
    assert(vm.current_address() == 18);

    printf("PASS: test_nop_instruction\n");
}

void test_end_of_script() {
    std::vector<uint8_t> bytecode(10, 0);

    SnrVm vm;
    vm.load_script(bytecode);

    TestCallbacks callbacks;
    assert(!vm.step(&callbacks));

    printf("PASS: test_end_of_script\n");
}

void test_load_script() {
    SnrVm vm;
    std::vector<uint8_t> bytecode(18, 0);
    vm.load_script(bytecode);

    assert(vm.current_address() == 0);
    assert(vm.is_running());

    printf("PASS: test_load_script\n");
}

void test_movie_play_instruction() {
    // Construct a bytecode with MOVIE opcode followed by a filename string
    // MOVIE = 0xb0, arg1 = offset to string
    // We'll put the string "test.mp4" right after the instruction

    uint8_t bytecode[36] = {0};
    // Instruction
    bytecode[0] = 0x00;       // flags
    bytecode[1] = 0xb0;       // opcode = MOVIE
    bytecode[2] = 18;         // arg1 = 18 (offset to string = right after instr)
    bytecode[6] = 0;          // arg2
    // String at offset 18: "test.mp4\0"
    const char* str = "test.mp4";
    for (int i = 0; str[i]; i++) {
        bytecode[18 + i] = str[i];
    }
    // Total: 18 + 9 = 27 bytes (padded to 36)

    SnrVm vm;
    vm.load_script(std::vector<uint8_t>(bytecode, bytecode + 36));

    TestCallbacks callbacks;
    bool cont = vm.step(&callbacks);
    assert(cont);
    assert(callbacks.movie_play_count == 1);
    assert(callbacks.last_movie_file == "test.mp4");

    printf("PASS: test_movie_play_instruction\n");
}

int main() {
    test_instruction_size();
    test_decode_instruction();
    test_wait_instruction();
    test_nop_instruction();
    test_end_of_script();
    test_load_script();
    test_movie_play_instruction();

    printf("\nAll SnrVm tests passed!\n");
    return 0;
}
