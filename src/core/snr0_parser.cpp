#include "snr0_parser.hpp"
#include <cstring>
#include <fstream>
#include <algorithm>
#include <stdexcept>

namespace entergram {

// =============================================================================
// SNR Format Reference (reverse-engineered from Umineko/Higurashi Switch data.rom):
//
// Header (16+ bytes):
//   offset 0x00: magic[4] = "SNR0" (0x30524E53) or "SNR " (0x20524E53)
//   offset 0x04: version (u32) — timestamp for "SNR " format
//   offset 0x08: data_size_offset (u32) — meaning depends on variant
//   offset 0x0C: string_count (u32) — number of strings (for "SNR ")
//   offset 0x10: data_offset (u32) — file offset to data
//
// "SNR0" format (Umineko): data is LZSS-compressed
// "SNR " format (Higurashi): data is raw/uncompressed
//
// =============================================================================

static constexpr uint32_t SNR0_MAGIC = 0x30524E53; // "SNR0"
static constexpr uint32_t SNR_SPACE_MAGIC = 0x20524E53; // "SNR "

bool Snr0Parser::is_valid_snr0(const std::vector<uint8_t>& data) {
    if (data.size() < 16) return false;

    uint32_t magic;
    std::memcpy(&magic, data.data(), 4);
    return magic == SNR0_MAGIC || magic == SNR_SPACE_MAGIC;
}

std::vector<uint8_t> Snr0Parser::decompress(
    const uint8_t* compressed,
    size_t compressed_size,
    size_t uncompressed_size) {

    std::vector<uint8_t> output;
    output.reserve(uncompressed_size);

    const uint8_t* ptr = compressed;
    const uint8_t* end = compressed + compressed_size;

    while (ptr < end && output.size() < uncompressed_size) {
        uint8_t tag = *ptr++;
        if (ptr >= end) break;

        if (tag == 0) {
            // Literal run: next byte is count, followed by that many literal bytes
            uint8_t count = *ptr++;
            for (uint8_t i = 0; i < count && ptr < end && output.size() < uncompressed_size; i++) {
                output.push_back(*ptr++);
            }
        } else {
            // Back reference: tag encodes length and offset
            // High nibble = length (minus 1), low nibble * 256 + next byte = distance
            uint8_t length = (tag >> 4) + 1;
            uint8_t offset_lo = *ptr++;
            uint16_t distance = (uint16_t)(tag & 0x0F) * 256 + offset_lo;

            if (distance == 0 || distance > output.size()) {
                // Invalid back reference — break to avoid infinite loop
                break;
            }

            for (uint8_t i = 0; i < length; i++) {
                if (output.size() >= uncompressed_size) break;
                output.push_back(output[output.size() - distance]);
            }
        }
    }

    return output;
}

std::vector<uint8_t> Snr0Parser::decompress_block(
    const uint8_t* data,
    size_t size) {

    // For a single block, we just decompress directly
    return decompress(data, size, SIZE_MAX);
}

std::vector<uint8_t> Snr0Parser::parse(const std::vector<uint8_t>& file_data) {
    if (!is_valid_snr0(file_data)) {
        throw std::runtime_error("Invalid SNR0 file: bad magic");
    }

    if (file_data.size() < 16) {
        throw std::runtime_error("SNR0 file too small");
    }

    // Detect SNR format variant
    uint32_t magic;
    std::memcpy(&magic, file_data.data(), 4);
    bool is_snr_space = (magic == SNR_SPACE_MAGIC);

    // Parse header fields
    uint32_t field_04, field_08, field_0c, field_10 = 0;
    std::memcpy(&field_04, &file_data[4], 4);
    std::memcpy(&field_08, &file_data[8], 4);
    std::memcpy(&field_0c, &file_data[12], 4);
    // field at 0x10 may or may not be present
    if (file_data.size() >= 20) {
        std::memcpy(&field_10, &file_data[16], 4);
    }

    // "SNR " format (Higurashi): raw/uncompressed bytecode
    //   0x00: "SNR " magic (4 bytes)
    //   0x04: timestamp (4 bytes)
    //   0x08: string_table_offset — offset to string table (bytecode end)
    //   0x0C: string_count — number of strings (63 for Higurashi)
    //   0x10: padding/reserved (0x81)
    //   0x14-0x1F: padding (8 bytes zeros)
    //   Bytecode: bytes [0x20 .. string_table_offset]
    //   String table: bytes [string_table_offset .. end]
    if (is_snr_space) {
        size_t bytecode_start = 32;  // After 16-byte header + 16 bytes padding
        size_t string_table_offset = field_08;

        // Bytecode is between header padding and string table
        if (string_table_offset > bytecode_start &&
            string_table_offset <= file_data.size()) {
            return std::vector<uint8_t>(
                file_data.begin() + bytecode_start,
                file_data.begin() + string_table_offset
            );
        }

        // Fallback: from offset 32 to end
        return std::vector<uint8_t>(
            file_data.begin() + bytecode_start,
            file_data.end()
        );
    }

    // SNR0 format (Umineko): data may be compressed
    // field_08 = compressed_size, field_0c = uncompressed_size
    uint32_t compressed_size = field_08;
    uint32_t uncompressed_size = field_0c;

    // If data is stored uncompressed
    if (uncompressed_size == 0 || uncompressed_size == compressed_size) {
        size_t start = 16;
        size_t len = std::min((size_t)compressed_size, file_data.size() - start);
        if (len > 0) {
            return std::vector<uint8_t>(
                file_data.begin() + start,
                file_data.begin() + start + len
            );
        }
        return std::vector<uint8_t>(file_data.begin() + 16, file_data.end());
    }

    // Decompress the data after the header
    const uint8_t* compressed_data = file_data.data() + 16;
    size_t comp_size = std::min((size_t)compressed_size, file_data.size() - 16);
    if (comp_size == 0) comp_size = file_data.size() - 16;

    auto result = decompress(compressed_data, comp_size, uncompressed_size);
    return result;
}

std::optional<std::vector<uint8_t>> Snr0Parser::parse_file(const std::string& file_path) {
    std::ifstream f(file_path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) {
        return std::nullopt;
    }

    size_t size = f.tellg();
    f.seekg(0, std::ios::beg);

    std::vector<uint8_t> data(size);
    f.read(reinterpret_cast<char*>(data.data()), size);
    f.close();

    try {
        return parse(data);
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace entergram
