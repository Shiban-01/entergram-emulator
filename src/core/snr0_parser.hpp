#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <cstddef>

namespace entergram {

// Forward declaration
class SnrVm;

// Snr0Parser: parses Higurashi Switch "SNR " format.
// The file layout is:
//   header(32 bytes): magic, timestamp, string_table_offset, string_count, jump_table_count, padding
//   jump table: jump_table_count × u32 (first entry = bytecode_start offset)
//   string table entries + bytecode region
// Bytecode references strings via absolute file offsets.
// The parser returns a struct with bytecode buffer + the full file data for string resolution.
struct ParsedSnr {
    std::vector<uint8_t> bytecode;  // actual bytecode region
    const std::vector<uint8_t>* file_data;  // full SNR file data (for string table resolution)
    uint32_t bytecode_start;  // absolute file offset where bytecode begins
    uint32_t string_table_offset;  // absolute file offset where string table begins
};

class Snr0Parser {
public:
    Snr0Parser() = default;
    ~Snr0Parser() = default;

    // Returns a ParsedSnr with bytecode and file data for string resolution
    static ParsedSnr parse_full(const std::vector<uint8_t>& file_data);

    static std::vector<uint8_t> parse(const std::vector<uint8_t>& file_data);
    static std::optional<std::vector<uint8_t>> parse_file(const std::string& file_path);
    static bool is_valid_snr0(const std::vector<uint8_t>& data);

    // Decompressor (public for testing)
    static std::vector<uint8_t> decompress(
        const uint8_t* compressed,
        size_t compressed_size,
        size_t uncompressed_size
    );

private:
    static std::vector<uint8_t> decompress_block(
        const uint8_t* data,
        size_t size
    );
};

} // namespace entergram
