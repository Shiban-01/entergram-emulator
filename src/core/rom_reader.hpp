#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <fstream>
#include <bit>
#include <stdexcept>
#include <cassert>
#include <cstring>

namespace entergram {

// ROM2 file header (16 bytes at offset 0, index at offset 0x20)
struct RomHeader {
    char magic[4];                // "ROM2" = 0x324D4F52 little-endian
    uint32_t version;             // 0x00010001
    uint32_t index_length;        // Index section length in bytes
    uint32_t offset_multiplier;   // Multiply index data_offset by this
    // bytes 16-31: padding/reserved, INDEX starts at offset 0x20

    static constexpr uint32_t EXPECTED_MAGIC = 0x324D4F52;
    static constexpr uint32_t EXPECTED_VERSION = 0x00010001;
    static constexpr uint32_t INDEX_OFFSET = 0x20;
};

// B-tree directory entry
struct RomEntry {
    std::string name;
    uint64_t data_offset;    // Offset into ROM, multiply by offset_multiplier
    uint32_t data_size;      // Size in bytes (0 for directories)
    bool is_directory;
    std::vector<RomEntry> children;
    uint32_t _entries_offset; // For directories: offset of subdir entries (× 16)

    bool is_file() const { return !is_directory; }
};

// Result of extracting a file
struct ExtractedFile {
    std::string path;
    std::vector<uint8_t> data;
};

// ROM2 reader — parses the binary ROM2 format used by Entergram games.
class RomReader {
public:
    RomReader();
    ~RomReader();

    // Open a ROM2 file from disk (validates magic)
    bool open(const std::string& rom_path);

    // Parse the ROM header and build the directory tree
    bool parse();

    // Get the root directory (throws if parse() not called)
    const RomEntry& root() const;

    // Extract a file at the given path (e.g. "voice/01/001.nxa")
    std::optional<ExtractedFile> extract_file(const std::string& path) const;

    // Iterate all file paths recursively
    std::vector<std::string> list_all_files() const;

    // ROM stats
    size_t total_file_count() const { return total_files_; }
    size_t total_directory_count() const { return total_dirs_; }

    // Accessors
    const RomHeader& header() const { return header_; }

    // Utility: split a path into components
    static std::vector<std::string> split_path(const std::string& path);

    // Utility: find an entry by name in a directory (public for testing)
    static std::optional<const RomEntry*> find_entry(
        const RomEntry& dir, const std::string& name);

    // Find a directory entry by path (e.g. "movie", "movie/intro")
    std::optional<const RomEntry*> find_directory(const std::string& path) const;
    // Find a directory recursively from a parent entry
    static std::optional<const RomEntry*> find_directory_in(
        const RomEntry& parent, const std::vector<std::string>& parts);
    static std::optional<const RomEntry*> find_directory_in(
        const RomEntry& parent, const std::string& path);

private:
    std::string rom_path_;
    std::vector<uint8_t> index_data_;
    RomHeader header_{};
    std::unique_ptr<RomEntry> root_entry_;
    size_t total_files_ = 0;
    size_t total_dirs_ = 0;

    // Parse a single 12-byte directory entry
    RomEntry parse_entry_at(const uint8_t* ptr, uint32_t current_dir_offset) const;

    // Recursively parse directory entries within the index
    RomEntry parse_directory(uint32_t dir_file_offset, uint32_t current_dir_offset);
    void parse_directory_recursive(RomEntry& dir_entry, uint32_t dir_file_offset, uint32_t current_dir_offset, int depth);

    // Read raw data at (data_offset * offset_multiplier)
    std::vector<uint8_t> read_data(uint64_t data_offset, uint32_t data_size) const;

    // Recursively count files and directories
    void count_entries(const RomEntry& entry);

    // Recursively list all file paths
    void list_files_recursive(const RomEntry& entry, const std::string& prefix,
                              std::vector<std::string>& out) const;
};

} // namespace entergram
