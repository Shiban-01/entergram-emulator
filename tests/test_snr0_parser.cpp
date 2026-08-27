#include "../src/core/snr0_parser.hpp"
#include <iostream>
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace entergram;

void test_valid_magic() {
    // Create a valid SNR0 header
    uint8_t data[16] = {0};
    uint32_t magic = 0x30524E53; // "SNR0"
    std::memcpy(data, &magic, 4);

    assert(Snr0Parser::is_valid_snr0(std::vector<uint8_t>(data, data + 16)));
    printf("PASS: test_valid_magic\n");
}

void test_invalid_magic() {
    uint8_t data[16] = {0};
    uint32_t magic = 0x12345678;
    std::memcpy(data, &magic, 4);

    assert(!Snr0Parser::is_valid_snr0(std::vector<uint8_t>(data, data + 16)));
    printf("PASS: test_invalid_magic\n");
}

void test_empty_data() {
    std::vector<uint8_t> data;
    assert(!Snr0Parser::is_valid_snr0(data));
    printf("PASS: test_empty_data\n");
}

void test_decompress_literal() {
    // Tag 0 = literal run, next byte = count, then count literal bytes
    std::vector<uint8_t> compressed;
    compressed.push_back(0x00);     // Literal tag
    compressed.push_back(0x05);     // 5 bytes follow
    compressed.push_back('H');
    compressed.push_back('e');
    compressed.push_back('l');
    compressed.push_back('l');
    compressed.push_back('o');

    auto result = Snr0Parser::decompress(compressed.data(), compressed.size(), 5);
    assert(result.size() == 5);
    assert(std::memcmp(result.data(), "Hello", 5) == 0);

    printf("PASS: test_decompress_literal\n");
}

void test_decompress_back_reference() {
    // First: literal "ABC"
    // Then: back reference repeating "ABC" 3 times
    std::vector<uint8_t> compressed;
    compressed.push_back(0x00);     // Literal tag
    compressed.push_back(0x03);     // 3 bytes
    compressed.push_back('A');
    compressed.push_back('B');
    compressed.push_back('C');
    // Back reference: length=3 (tag 0x30 = (3-1)<<4), distance=3 (0x03 * 256 + 0x00)
    // Actually, distance = (tag & 0x0F) * 256 + next_byte
    // For distance=3: tag_low = 0, offset_lo = 3
    // tag = 0x30 | 0x00 = 0x30, then offset_lo = 3
    compressed.push_back(0x30);     // length=3, distance low nibble=0
    compressed.push_back(0x03);     // distance = 0*256 + 3 = 3

    // Actually the back reference logic in our code:
    // length = (tag >> 4) + 1 = 3
    // distance = (tag & 0x0F) * 256 + offset_lo = 0*256 + 3 = 3
    // Copy 3 bytes from 3 positions back: "ABC"

    // Wait, we need more output bytes. Let me reconsider.
    // Output after literals: "ABC" (3 bytes)
    // Back reference: copy 3 bytes from distance 3 → "ABC"
    // Total expected: "ABCABC" (6 bytes)

    auto result = Snr0Parser::decompress(compressed.data(), compressed.size(), 6);
    assert(result.size() == 6);
    assert(std::memcmp(result.data(), "ABCABC", 6) == 0);

    printf("PASS: test_decompress_back_reference\n");
}

void test_parse_stored_uncompressed() {
    // Create a valid SNR0 file with stored (uncompressed) data
    std::vector<uint8_t> file_data(20 + 5, 0);
    uint32_t magic = 0x30524E53;
    std::memcpy(file_data.data(), &magic, 4);
    uint32_t version = 1;
    std::memcpy(&file_data[4], &version, 4);
    uint32_t comp_size = 5;
    std::memcpy(&file_data[8], &comp_size, 4);
    uint32_t uncomp_size = 5;
    std::memcpy(&file_data[12], &uncomp_size, 4);
    uint32_t decomp_offset = 20;
    std::memcpy(&file_data[16], &decomp_offset, 4);
    // Data at offset 20: "HELLO"
    std::memcpy(&file_data[20], "HELLO", 5);

    auto result = Snr0Parser::parse(file_data);
    assert(result.size() == 5);
    assert(std::memcmp(result.data(), "HELLO", 5) == 0);

    printf("PASS: test_parse_stored_uncompressed\n");
}

int main() {
    test_valid_magic();
    test_invalid_magic();
    test_empty_data();
    test_decompress_literal();
    test_decompress_back_reference();
    test_parse_stored_uncompressed();

    printf("\nAll Snr0Parser tests passed!\n");
    return 0;
}
