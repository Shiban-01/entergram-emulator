#include "../src/core/snr_vm.hpp"
#include <iostream>
#include <cassert>
#include <cstdio>

using namespace entergram;

class TestCallbacks : public SnrVmCallbacks {
public:
    int voice_play_count = 0;
    int movie_play_count = 0;
    int text_count = 0;
    int wait_count = 0;
    int choice_count = 0;
    std::string last_voice_file;
    std::string last_movie_file;
    std::string last_text;
    int last_wait_frames = 0;
    std::vector<std::string> last_options;

    void on_voice_play(const std::string& file_name, int volume, int) override {
        voice_play_count++;
        last_voice_file = file_name;
    }
    void on_movie_play(const std::string& file_name) override {
        movie_play_count++;
        last_movie_file = file_name;
    }
    void on_text_display(const std::string& text, const std::string&) override {
        text_count++;
        last_text = text;
    }
    void on_wait(int frames) override {
        wait_count++;
        last_wait_frames = frames;
    }
    void on_choice(const std::vector<std::string>& options) override {
        choice_count++;
        last_options = options;
    }
};

void test_wait_instruction() {
    // WAIT (0x83) + VarInt(30)
    // 30 = 0x1e, which is < 0x80, so it's a 7-bit signed constant
    uint8_t bytecode[] = {0x83, 0x1e};
    SnrVm vm;
    vm.load_script(std::vector<uint8_t>(bytecode, bytecode + 2));
    TestCallbacks callbacks;
    bool yielded = vm.step(&callbacks);
    assert(!yielded);  // WAIT yields — step returns false
    assert(callbacks.wait_count == 1);
    assert(callbacks.last_wait_frames == 30);
    assert(vm.is_running());
    printf("PASS: test_wait_instruction\n");
}

void test_jmp_instruction() {
    // JMP (0x47) + offset (u32 LE) = jump to offset 4
    // Bytecode: [JMP][target=4][NOP][EXIT]
    // Actually test: JMP to offset 0 (unconditional) then EXIT
    uint8_t bytecode[] = {
        0x47, 0x04, 0x00, 0x00, 0x00,  // JMP to offset 4
        0x00,                          // EXIT at offset 4
        0x00                           // number arg for EXIT
    };
    SnrVm vm;
    vm.load_script(std::vector<uint8_t>(bytecode, bytecode + 6));
    TestCallbacks callbacks;
    bool cont = vm.step(&callbacks);
    assert(cont);  // JMP continues
    assert(vm.current_address() == 4);
    // Next step: EXIT (0x00)
    vm.step(&callbacks);
    assert(!vm.is_running());
    printf("PASS: test_jmp_instruction\n");
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

void test_varint_decoding() {
    // 7-bit constant positive
    uint8_t data1[] = {0x05};
    size_t pos = 0;
    int64_t v = SnrVm::read_varint(data1, pos);
    assert(v == 5);
    assert(pos == 1);

    // 7-bit constant negative (0x80 is not signed... let's test 0x06)
    uint8_t data2[] = {0x06};
    pos = 0;
    v = SnrVm::read_varint(data2, pos);
    assert(v == 6);

    // 12-bit constant: 0x80 | 0x05 (P=0), then 0x01
    // P = (0x85 & 0x70) >> 4 = (0x05) >> 4 = 0
    // val = (0x05 << 8) | 0x01 = 0x501 = 1281
    uint8_t data3[] = {0x85, 0x01};
    pos = 0;
    v = SnrVm::read_varint(data3, pos);
    assert(v == 0x501);
    assert(pos == 2);

    // 4-bit register: 0xB0 (P=3, reg=0)
    uint8_t data4[] = {0xB0};
    pos = 0;
    v = SnrVm::read_varint(data4, pos);
    assert(v == 0);
    assert(pos == 1);

    printf("PASS: test_varint_decoding\n");
}

void test_choice_instruction() {
    // SELECT (0x8d): u16 + u16 + reg + number + string(U8) + string_array(U8)
    // cap="Yes", opts=["Yes", "No"]
    uint8_t bytecode[] = {
        0x8d,                          // SELECT
        0x01, 0x00,                    // u16 = 1
        0x02, 0x00,                    // u16 = 2
        0x00,                          // VarInt reg = 0
        0x00,                          // VarInt number = 0
        0x03,                          // string len = 3
        'Y','e','s',                   // "Yes"
        0x00,                          // null terminator
        0x02,                          // string array count = 2
        0x03, 'Y','e','s', 0x00,       // "Yes"
        0x02, 'N','o', 0x00            // "No"
    };
    SnrVm vm;
    vm.load_script(std::vector<uint8_t>(bytecode, bytecode + sizeof(bytecode)));
    TestCallbacks callbacks;
    bool yielded = vm.step(&callbacks);
    assert(!yielded);  // SELECT yields
    assert(callbacks.choice_count == 1);
    assert(callbacks.last_options.size() == 2);
    assert(callbacks.last_options[0] == "Yes");
    assert(callbacks.last_options[1] == "No");
    printf("PASS: test_choice_instruction\n");
}

int main() {
    test_varint_decoding();
    test_load_script();
    test_end_of_script();
    test_wait_instruction();
    test_jmp_instruction();
    test_choice_instruction();
    printf("\nAll SnrVm tests passed!\n");
    return 0;
}
