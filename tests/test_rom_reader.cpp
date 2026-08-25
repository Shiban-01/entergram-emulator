#include "../src/core/rom_reader.hpp"
#include <iostream>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <cstdint>

using namespace entergram;

void test_header_parsing() {
    // Test that the header struct matches expected layout
    static_assert(sizeof(RomHeader) == 16, "RomHeader should be 16 bytes");
    static_assert(offsetof(RomHeader, magic) == 0, "magic at offset 0");
    static_assert(offsetof(RomHeader, version) == 4, "version at offset 4");
    static_assert(offsetof(RomHeader, index_length) == 8, "index_length at offset 8");
    static_assert(offsetof(RomHeader, offset_multiplier) == 12, "offset_multiplier at offset 12");

    printf("PASS: test_header_parsing\n");
}

void test_split_path() {
    auto parts = RomReader::split_path("voice/01/001.nxa");
    assert(parts.size() == 3);
    assert(parts[0] == "voice");
    assert(parts[1] == "01");
    assert(parts[2] == "001.nxa");

    parts = RomReader::split_path("main.snr");
    assert(parts.size() == 1);
    assert(parts[0] == "main.snr");

    printf("PASS: test_split_path\n");
}

void test_find_entry() {
    RomEntry dir;
    dir.is_directory = true;
    dir.children.push_back({"subdir", 100, 0, true, {}});
    dir.children.push_back({"file.txt", 200, 42, false, {}});

    auto found = RomReader::find_entry(dir, "file.txt");
    assert(found);
    assert((*found)->data_size == 42);

    auto not_found = RomReader::find_entry(dir, "nonexistent");
    assert(!not_found);

    printf("PASS: test_find_entry\n");
}

void test_open_nonexistent() {
    RomReader reader;
    assert(!reader.open("nonexistent_file.rom"));
    printf("PASS: test_open_nonexistent\n");
}

void test_open_invalid_format(const std::string& temp_path) {
    // Create a fake file with wrong magic
    std::ofstream f(temp_path, std::ios::binary);
    uint32_t bad_magic = 0x12345678;
    f.write(reinterpret_cast<const char*>(&bad_magic), 4);
    f.write("\x00\x01\x00\x01", 4);  // version
    uint32_t index_len = 0;
    f.write(reinterpret_cast<const char*>(&index_len), 4);
    uint32_t mult = 512;
    f.write(reinterpret_cast<const char*>(&mult), 4);
    f.close();

    RomReader reader;
    assert(!reader.open(temp_path));
    printf("PASS: test_open_invalid_format\n");
}

int main() {
    test_header_parsing();
    test_split_path();
    test_find_entry();
    test_open_nonexistent();
    {
        std::string temp_path = "test_temp_invalid.rom";
        test_open_invalid_format(temp_path);
        std::remove(temp_path.c_str());
    }
    printf("\nAll RomReader tests passed!\n");
    return 0;
}
