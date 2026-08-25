#include "rom_reader.hpp"
#include <cstring>
#include <algorithm>

namespace entergram {

// =============================================================================
// ROM2 Format Reference (reverse-engineered from Umineko Switch data.rom):
//
// Header (32 bytes, INDEX at offset 0x20):
//   offset 0x00: magic[4] = "ROM2" (0x324D4F52 little-endian)
//   offset 0x04: version = 0x00010001
//   offset 0x08: index_length = 2,762,608 bytes (index section size)
//   offset 0x0C: offset_multiplier = 512 (data offsets in index × this)
//   offset 0x10-0x1F: padding/reserved
//
// Index Section (starts at 0x20):
//   B-tree directory structure. Each entry is 12 bytes:
//     name_offset (u32): bit 31 = is_directory flag; lower 31 bits = offset to name string
//     data_offset   (u32): offset into the data area (× offset_multiplier)
//     data_size     (u32): file size in bytes (0 for directories)
//
// Name Table: Strings stored within the index section,
// referenced by absolute offset from the start of the index data.
// =============================================================================

RomReader::RomReader() = default;

RomReader::~RomReader() = default;

bool RomReader::open(const std::string& rom_path) {
    rom_path_ = rom_path;

    // Read the entire file to validate header (we only need the first 32 bytes)
    std::ifstream f(rom_path, std::ios::binary);
    if (!f.is_open()) {
        return false;
    }

    // Read 16 bytes (we only need to validate magic + basic fields)
    f.read(reinterpret_cast<char*>(&header_), sizeof(uint32_t) * 4);
    if (!f) {
        return false;
    }

    f.close();

    // Validate magic
    uint32_t magic_u32;
    std::memcpy(&magic_u32, header_.magic, 4);
    if (magic_u32 != RomHeader::EXPECTED_MAGIC) {
        return false;
    }

    return true;
}

bool RomReader::parse() {
    // Open file for reading
    std::ifstream f(rom_path_, std::ios::binary);
    if (!f.is_open()) {
        return false;
    }

    // Read header
    f.read(reinterpret_cast<char*>(&header_), sizeof(RomHeader));
    if (!f) {
        f.close();
        return false;
    }

    // Validate magic
    uint32_t magic_u32;
    std::memcpy(&magic_u32, header_.magic, 4);
    if (magic_u32 != RomHeader::EXPECTED_MAGIC) {
        f.close();
        return false;
    }

    // Read the entire index section into memory
    // Index starts at INDEX_OFFSET (0x20), length = header_.index_length
    index_data_.resize(header_.index_length);
    f.seekg(RomHeader::INDEX_OFFSET, std::ios::beg);
    f.read(reinterpret_cast<char*>(index_data_.data()), header_.index_length);
    if (!f) {
        f.close();
        return false;
    }

    f.close();

    // Parse root directory
    root_entry_ = std::make_unique<RomEntry>(
        parse_directory(index_data_.data(), header_.index_length));

    count_entries(*root_entry_);

    return true;
}

RomEntry RomReader::parse_entry_at(const uint8_t* ptr) const {
    RomEntry entry;

    // Read name_offset (u32, little-endian)
    uint32_t name_offset_and_flag;
    std::memcpy(&name_offset_and_flag, ptr, 4);
    ptr += 4;

    // Read data_offset (u32)
    uint32_t data_offset;
    std::memcpy(&data_offset, ptr, 4);
    ptr += 4;

    // Read data_size (u32)
    uint32_t data_size;
    std::memcpy(&data_size, ptr, 4);

    // Extract name
    bool is_dir = (name_offset_and_flag & 0x80000000) != 0;
    uint32_t name_offset = name_offset_and_flag & 0x7FFFFFFF;

    // Names are stored in the index section, referenced by absolute offset
    const char* name_str = reinterpret_cast<const char*>(index_data_.data() + name_offset);
    entry.name = name_str;

    entry.data_offset = data_offset;
    entry.data_size = data_size;
    entry.is_directory = is_dir;

    return entry;
}

RomEntry RomReader::parse_directory(const uint8_t* data, size_t length) {
    RomEntry dir_entry;
    dir_entry.name = "/";
    dir_entry.is_directory = true;

    const uint8_t* ptr = data;
    const uint8_t* end = data + length;

    while (ptr + 12 <= end) {
        RomEntry entry = parse_entry_at(ptr);

        // Check for end marker (null name or zero length)
        if (entry.name.empty() || entry.name[0] == '\0') {
            break;
        }

        if (entry.is_directory) {
            // Parse subdirectory: entries stored at (data_offset * offset_multiplier)
            uint64_t sub_offset = (uint64_t)entry.data_offset * header_.offset_multiplier;
            if (sub_offset < index_data_.size()) {
                const uint8_t* sub_data = index_data_.data() + sub_offset;
                size_t sub_length = index_data_.size() - sub_offset;
                entry = parse_directory(sub_data, sub_length);
            }
        }

        dir_entry.children.push_back(std::move(entry));
        ptr += 12;
    }

    return dir_entry;
}

std::vector<uint8_t> RomReader::read_data(uint64_t data_offset, uint32_t data_size) const {
    uint64_t abs_offset = (uint64_t)data_offset * header_.offset_multiplier;
    std::vector<uint8_t> buffer(data_size);

    std::ifstream f(rom_path_, std::ios::binary);
    if (!f.is_open()) {
        return {};
    }

    f.seekg(abs_offset, std::ios::beg);
    f.read(reinterpret_cast<char*>(buffer.data()), data_size);
    if (!f) {
        return {};
    }

    return buffer;
}

std::vector<std::string> RomReader::split_path(const std::string& path) {
    std::vector<std::string> parts;
    size_t start = 0;
    size_t end = path.find('/');

    while (end != std::string::npos) {
        parts.push_back(path.substr(start, end - start));
        start = end + 1;
        end = path.find('/', start);
    }
    if (start < path.size()) {
        parts.push_back(path.substr(start));
    }

    return parts;
}

std::optional<const RomEntry*> RomReader::find_entry(
    const RomEntry& dir, const std::string& name) {
    for (const auto& child : dir.children) {
        if (child.name == name) {
            return &child;
        }
    }
    return std::nullopt;
}

const RomEntry& RomReader::root() const {
    if (!root_entry_) {
        throw std::runtime_error("RomReader: parse() was not called");
    }
    return *root_entry_;
}

std::optional<ExtractedFile> RomReader::extract_file(const std::string& path) const {
    auto parts = split_path(path);
    const RomEntry* current = &root();

    for (const auto& part : parts) {
        auto found = find_entry(*current, part);
        if (!found) {
            return std::nullopt;
        }
        current = *found;
    }

    if (current->is_directory) {
        return std::nullopt;
    }

    auto data = read_data(current->data_offset, current->data_size);
    if (data.size() != current->data_size) {
        return std::nullopt;
    }
    return ExtractedFile{path, std::move(data)};
}

void RomReader::count_entries(const RomEntry& entry) {
    if (!entry.is_directory) {
        total_files_++;
    } else {
        if (!entry.name.empty() && entry.name != "/") {
            total_dirs_++;
        }
        for (const auto& child : entry.children) {
            count_entries(child);
        }
    }
}

void RomReader::list_files_recursive(
    const RomEntry& entry,
    const std::string& prefix,
    std::vector<std::string>& out) const {

    for (const auto& child : entry.children) {
        std::string full_path = prefix.empty() ? child.name : prefix + "/" + child.name;

        if (child.is_directory) {
            list_files_recursive(child, full_path, out);
        } else {
            out.push_back(full_path);
        }
    }
}

std::vector<std::string> RomReader::list_all_files() const {
    std::vector<std::string> files;
    if (root_entry_) {
        list_files_recursive(*root_entry_, "", files);
    }
    return files;
}

} // namespace entergram
