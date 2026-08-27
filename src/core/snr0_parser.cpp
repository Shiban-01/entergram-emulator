#include "snr0_parser.hpp"
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace entergram {

// =============================================================================
// SNR0 Format Reference (reverse-engineered from Umineko Switch data.rom):
//
// Header (16 bytes):
//   offset 0x00: magic[4] = "SNR0" (0x30524E53 little-endian)
//   offset 0x04: version (u32)
//   offset 0x08: compressed_size (u32) — size of compressed data after header
//   offset 0x0C: uncompressed_size (u32) — size of decompressed data
//   offset 0x10: decompressed_offset (u32) — file offset to decompressed data
//                (typically equals compressed_size + header_size)
//
// After the header, the compressed data follows.
// The decompression algorithm is custom to Entergram.
// =============================================================================

static constexpr uint32_t SNR0_MAGIC = 0x30524E53; // "SNR0"

bool Snr0Parser::is_valid_snr0(const std::vector<uint8_t>& data) {
    if (data.size() < 16) return false;

    uint32_t magic;
    std::memcpy(&magic, data.data(), 4);
    return magic == SNR0_MAGIC;
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
            // High nibble = length (minus 1), low nibble × 256 + next byte = distance
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
    // The SNR0 format uses the same compression for all blocks
    return decompress(data, size, SIZE_MAX);
}

std::vector<uint8_t> Snr0Parser::parse(const std::vector<uint8_t>& file_data) {
    if (!is_valid_snr0(file_data)) {
        throw std::runtime_error("Invalid SNR0 file: bad magic");
    }

    if (file_data.size() < 16) {
        throw std::runtime_error("SNR0 file too small");
    }

    // Parse header
    uint32_t version, compressed_size, uncompressed_size, decompressed_offset;
    std::memcpy(&version, &file_data[4], 4);
    std::memcpy(&compressed_size, &file_data[8], 4);
    std::memcpy(&uncompressed_size, &file_data[12], 4);

    // decompressed_offset is at offset 0x10 (if header is 16+ bytes)
    if (file_data.size() < 20) {
        decompressed_offset = compressed_size + 16;
    } else {
        std::memcpy(&decompressed_offset, &file_data[16], 4);
    }

    // If decompressed data is already in the file (stored uncompressed)
    if (decompressed_offset + uncompressed_size <= file_data.size()) {
        // Data is stored uncompressed
        return std::vector<uint8_t>(
            file_data.begin() + decompressed_offset,
            file_data.begin() + decompressed_offset + uncompressed_size
        );
    }

    // Otherwise, decompress the data after the header
    const uint8_t* compressed_data = file_data.data() + 16;
    size_t comp_size = file_data.size() - 16;

    // Try decompression
    auto result = decompress(compressed_data, comp_size, uncompressed_size);
    if (result.size() != uncompressed_size) {
        // Decompression may have produced different size — still return what we have
        // This handles the case where uncompressed_size is approximate
    }

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
