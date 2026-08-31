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

ParsedSnr Snr0Parser::parse_full(const std::vector<uint8_t>& file_data) {
    ParsedSnr result;
    result.file_data = &file_data;

    if (!is_valid_snr0(file_data) || file_data.size() < 32) {
        // Not a valid SNR, or too small for SNR-space header.
        // For SNR0 format, parse() handles the SNR0-specific header (20 bytes).
        result.bytecode = parse(file_data);
        result.bytecode_start = 16;
        result.string_table_offset = file_data.size();
        return result;
   }
    // For SNR space format, verify it actually IS SNR space
    uint32_t magic_check;
    std::memcpy(&magic_check, file_data.data(), 4);
    if (magic_check != SNR_SPACE_MAGIC) {
        // SNR0 format
        result.bytecode = parse(file_data);
        result.bytecode_start = 16;
        result.string_table_offset = file_data.size();
        return result;
    }

    uint32_t field_08 = 0, field_0c = 0;
    std::memcpy(&field_08, &file_data[8], 4);   // string_table_offset
    std::memcpy(&field_0c, &file_data[12], 4); // string_count

    result.string_table_offset = field_08;

    // Read jump table[0] = bytecode_start offset (file offset)
    uint32_t bytecode_start = 0;
    std::memcpy(&bytecode_start, &file_data[32], 4);

    if (bytecode_start < 32 || bytecode_start >= field_08 || bytecode_start >= file_data.size()) {
        bytecode_start = 32;  // fallback
    }
    result.bytecode_start = bytecode_start;

    size_t st_off = field_08;
    if (st_off > file_data.size()) st_off = file_data.size();

    result.bytecode.assign(
        file_data.begin() + bytecode_start,
        file_data.begin() + bytecode_start + (st_off - bytecode_start)
    );
    return result;
}

std::vector<uint8_t> Snr0Parser::parse(const std::vector<uint8_t>& file_data) {
    if (!is_valid_snr0(file_data)) {
        throw std::runtime_error("Invalid SNR0 file: bad magic");
    }

    if (file_data.size() < 16) {
        throw std::runtime_error("SNR file too small for header");
    }

    // Detect SNR format variant
    uint32_t magic;
    std::memcpy(&magic, file_data.data(), 4);
    bool is_snr_space = (magic == SNR_SPACE_MAGIC);

    // Parse header fields
    uint32_t field_08 = 0, field_0c = 0, field_10 = 0;
    std::memcpy(&field_08, &file_data[8], 4);
    std::memcpy(&field_0c, &file_data[12], 4);
    if (file_data.size() >= 20) {
        std::memcpy(&field_10, &file_data[16], 4);
    }

    // "SNR " format (Higurashi Switch): raw/uncompressed bytecode
    //   0x00: "SNR " magic (4 bytes)
    //   0x04: timestamp (4 bytes)
    //   0x08: string_table_offset — file offset where strings begin (bytecode end)
    //   0x0C: string_count — number of string-table entries
    //   0x10: jump_table_count (field_c) — number of jump table entries
    //   0x14-0x1F: padding (zeros)
    //   0x20..: jump table (jump_table_count × u32) followed by
    //            string table entries, then the bytecode.
    //
    // IMPORTANT: jump table[0] = file offset of the actual bytecode.
    // The bytecode extends from jump_table[0] to string_table_offset.
    // String references inside the bytecode are absolute file offsets.
    if (is_snr_space) {
        size_t string_table_offset = field_08;
        size_t jump_table_start = 32;
        // Read first jump table entry = bytecode start offset
        if (jump_table_start + 4 > file_data.size()) {
            throw std::runtime_error("SNR file too small for jump table");
        }
        uint32_t bytecode_start = 0;
        std::memcpy(&bytecode_start, &file_data[jump_table_start], 4);

        if (bytecode_start < jump_table_start ||
            bytecode_start >= string_table_offset ||
            bytecode_start >= file_data.size()) {
            // Fallback: assume bytecode starts right after the jump table
            bytecode_start = jump_table_start;
        }

        // Validate string_table_offset
        if (string_table_offset > file_data.size()) {
            string_table_offset = file_data.size();
        }

        // Copy ONLY the bytecode region (not the string table).
        // The VM resolves string references via absolute SNR offsets.
        size_t bytecode_len = string_table_offset - bytecode_start;
        return std::vector<uint8_t>(
            file_data.begin() + bytecode_start,
            file_data.begin() + bytecode_start + bytecode_len
        );
    }

    // SNR0 format (Umineko): data may be compressed
    // field_08 = compressed_size, field_0c = uncompressed_size
    uint32_t compressed_size = field_08;
    uint32_t uncompressed_size = field_0c;

    // If data is stored uncompressed
    size_t data_start = 16;
    // field_10: for SNR0, offset 0x10 may contain decomp_offset (data start offset)
    if (field_10 > 0 && field_10 < file_data.size()) {
        data_start = field_10;
    }
    if (uncompressed_size == 0 || uncompressed_size == compressed_size) {
        size_t len = std::min((size_t)compressed_size, file_data.size() - data_start);
        if (len > 0) {
            return std::vector<uint8_t>(
                file_data.begin() + data_start,
                file_data.begin() + data_start + len
            );
        }
        return std::vector<uint8_t>(file_data.begin() + data_start, file_data.end());
    }

    // Decompress the data after the header
    const uint8_t* compressed_data = file_data.data() + data_start;
    size_t comp_size = std::min((size_t)compressed_size, file_data.size() - data_start);
    if (comp_size == 0) comp_size = file_data.size() - data_start;

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
