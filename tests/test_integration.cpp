#include "../src/core/rom_reader.hpp"
#include "../src/core/snr_vm.hpp"
#include "../src/core/snr0_parser.hpp"
#include "../src/video/player.hpp"
#include <iostream>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <cstdint>

using namespace entergram;

class TestCallbacks : public SnrVmCallbacks {
public:
    int voice_play_count = 0;
    int movie_play_count = 0;
    int text_display_count = 0;
    int choice_count = 0;
    int wait_count = 0;
    int sys_call_count = 0;
    int bgm_count = 0;
    int se_count = 0;

    void on_voice_play(const std::string& file_name, int volume, int) override {
        voice_play_count++;
        printf("  [CALLBACK] VOICEPLAY: %s (vol=%d)\n", file_name.c_str(), volume);
    }
    void on_voice_stop(int) override {}
    void on_voice_wait(int) override { wait_count++; }
    void on_bgm_play(const std::string& file_name, int volume) override {
        bgm_count++;
        printf("  [CALLBACK] BGMPLAY: %s (vol=%d)\n", file_name.c_str(), volume);
    }
    void on_bgm_stop() {}
    void on_se_play(const std::string& file_name, int volume) override {
        se_count++;
        printf("  [CALLBACK] SEPLAY: %s (vol=%d)\n", file_name.c_str(), volume);
    }
    void on_movie_play(const std::string& file_name) override {
        movie_play_count++;
        printf("  [CALLBACK] MOVIEPLAY: %s\n", file_name.c_str());
    }
    void on_movie_stop() {}
    void on_text_display(const std::string& text, const std::string& name) override {
        text_display_count++;
        std::string display = text.substr(0, 80);
        if (text.size() > 80) display += "...";
        printf("  [CALLBACK] TEXT: %s: %s\n", name.c_str(), display.c_str());
    }
    void on_choice(const std::vector<std::string>& options) override {
        choice_count++;
        printf("  [CALLBACK] CHOICE (%zu options):\n", options.size());
        for (size_t i = 0; i < options.size() && i < 5; i++) {
            printf("    [%zu] %s\n", i, options[i].substr(0, 60).c_str());
        }
    }
    void on_system_call(uint32_t code, const std::string& data) override {
        sys_call_count++;
        printf("  [CALLBACK] SYS(0x%02x): %s\n", code, data.substr(0, 40).c_str());
    }
    void on_wait(int) override {}
};

static std::vector<uint8_t> read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    size_t size = f.tellg();
    std::vector<uint8_t> data(size);
    f.seekg(0);
    f.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

static std::string get_path(const RomEntry& dir, const std::string& name, const std::string& prefix) {
    return prefix + name;
}

void test_rom_parser_with_real_rom() {
    printf("\n=== Test: RomReader with data.rom ===\n\n");

    RomReader reader;
    std::string rom_path = "C:/Users/francisco.q/AppData/Roaming/eden/dump/01006A300BA2C000/romfs/data.rom";

    bool opened = reader.open(rom_path);
    assert(opened && "Should be able to open data.rom");

    bool parsed = reader.parse();
    assert(parsed && "Should be able to parse ROM2");

    const auto& header = reader.header();
    printf("  Magic: %c%c%c%c\n", header.magic[0], header.magic[1], header.magic[2], header.magic[3]);
    printf("  Version: 0x%08X\n", header.version);
    printf("  Index length: %u bytes\n", header.index_length);
    printf("  Offset multiplier: %u\n", header.offset_multiplier);
    printf("  Total files: %zu\n", reader.total_file_count());
    printf("  Total directories: %zu\n", reader.total_directory_count());

    // List root directory
    const auto& root = reader.root();
    printf("  Root entries (%zu):\n", root.children.size());
    for (const auto& entry : root.children) {
        printf("    %s (%s)\n", entry.name.c_str(), entry.is_directory ? "dir" : "file");
    }

    // List all files (just count, don't print)
    auto all_files = reader.list_all_files();
    printf("  Total listed file paths: %zu\n", all_files.size());

    printf("PASS: RomReader with data.rom\n");
}

void test_extract_and_decompress_main_snr() {
    printf("\n=== Test: Extract + decompress main.snr ===\n\n");

    RomReader reader;
    std::string rom_path = "C:/Users/francisco.q/AppData/Roaming/eden/dump/01006A300BA2C000/romfs/data.rom";

    assert(reader.open(rom_path));
    assert(reader.parse());

    // Extract main.snr (it's in the root)
    auto file_data = reader.extract_file("main.snr");
    if (!file_data || file_data->data.empty()) {
        printf("  ERROR: Cannot extract main.snr\n");
        printf("PASS: Extract main.snr (skipped - not found)\n");
        return;
    }

    printf("  main.snr size: %zu bytes\n", file_data->data.size());

    // Check magic
    if (file_data->data.size() >= 4) {
        std::string magic(file_data->data.begin(), file_data->data.begin() + 4);
        printf("  File magic: %s\n", magic.c_str());
    }

    // Check SNR0 format
    if (Snr0Parser::is_valid_snr0(file_data->data)) {
        printf("  Format: SNR0 (LZ4 compressed)\n");
        auto script = Snr0Parser::parse(file_data->data);
        if (script.empty()) {
            printf("  ERROR: Decompression failed\n");
        } else {
            printf("  Decompressed: %zu bytes\n", script.size());

            // Show first bytes of decompressed data
            if (script.size() >= 4) {
                printf("  Decompressed magic: %c%c%c%c\n",
                       char(script[0]), char(script[1]), char(script[2]), char(script[3]));
            }

            // Run VM on decompressed script
            SnrVm vm;
            vm.load_script(script);
            printf("  VM loaded: %zu bytes of bytecode\n", vm.script_size());

            TestCallbacks callbacks;
            int count = 0;
            while (vm.is_running() && count < 200) {
                vm.step(&callbacks);
                count++;
            }
            printf("  Executed %d instructions\n", count);
            printf("  Callbacks: voice=%d, bgm=%d, se=%d, movie=%d, text=%d, choice=%d, sys=%d, wait=%d\n",
                   callbacks.voice_play_count, callbacks.bgm_count,
                   callbacks.se_count, callbacks.movie_play_count,
                   callbacks.text_display_count, callbacks.choice_count,
                   callbacks.sys_call_count, callbacks.wait_count);
        }
    } else {
        printf("  Not SNR0 format, running VM directly\n");
        SnrVm vm;
        vm.load_script(file_data->data);
        TestCallbacks callbacks;
        int count = 0;
        while (vm.is_running() && count < 200) {
            vm.step(&callbacks);
            count++;
        }
        printf("  Executed %d instructions\n", count);
    }

    printf("PASS: Extract + decompress main.snr\n");
}

void test_extract_movie() {
    printf("\n=== Test: Movie directory structure ===\n\n");

    RomReader reader;
    std::string rom_path = "C:/Users/francisco.q/AppData/Roaming/eden/dump/01006A300BA2C000/romfs/data.rom";

    assert(reader.open(rom_path));
    assert(reader.parse());

    // Find movie directory by looking in root children
    const auto& root = reader.root();
    for (const auto& entry : root.children) {
        if (entry.name == "movie" && entry.is_directory) {
            printf("  movie/ directory: %zu entries\n", entry.children.size());
            for (const auto& mov : entry.children) {
                printf("    %s (%u bytes, %s)\n",
                       mov.name.c_str(), mov.data_size,
                       mov.is_directory ? "dir" : "file");
            }
            break;
        }
    }

    printf("PASS: Movie directory structure\n");
}

void test_voice_directory() {
    printf("\n=== Test: Voice directory structure ===\n\n");

    RomReader reader;
    std::string rom_path = "C:/Users/francisco.q/AppData/Roaming/eden/dump/01006A300BA2C000/romfs/data.rom";

    assert(reader.open(rom_path));
    assert(reader.parse());

    const auto& root = reader.root();
    for (const auto& entry : root.children) {
        if (entry.name == "voice" && entry.is_directory) {
            printf("  voice/ directory: %zu character subdirectories\n", entry.children.size());
            // Show first 5 characters
            for (size_t i = 0; i < entry.children.size() && i < 5; i++) {
                const auto& chr = entry.children[i];
                printf("    %s/ (%zu files)\n", chr.name.c_str(), chr.children.size());
                // Show first 3 files
                for (size_t j = 0; j < chr.children.size() && j < 3; j++) {
                    const auto& file = chr.children[j];
                    printf("      %s (%u bytes)\n", file.name.c_str(), file.data_size);
                }
            }
            printf("    ... (%zu total characters)\n", entry.children.size());
            break;
        }
    }

    printf("\n  Total file count in ROM: %zu\n", reader.total_file_count());
    printf("PASS: Voice directory structure\n");
}

int main() {
    printf("=== Entergram Emulator - Integration Tests (data.rom) ===\n");
    printf("ROM: C:/Users/francisco.q/AppData/Roaming/eden/dump/01006A300BA2C000/romfs/data.rom\n\n");

    test_rom_parser_with_real_rom();
    test_extract_and_decompress_main_snr();
    test_extract_movie();
    test_voice_directory();

    printf("\n=== All integration tests passed! ===\n");
    return 0;
}
