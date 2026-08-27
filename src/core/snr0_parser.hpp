#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <cstddef>

namespace entergram {

class Snr0Parser {
public:
    Snr0Parser() = default;
    ~Snr0Parser() = default;

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
