#include "rom_reader.hpp"
#include <cstring>
#include <algorithm>
#include <stdexcept>

namespace entergram {

static constexpr uint32_t INDEX_OFFSET = 0x20;
static constexpr uint32_t DIRECTORY_OFFSET_MULTIPLIER = 16;

RomReader::RomReader() = default;
RomReader::~RomReader() = default;

bool RomReader::open(const std::string& rom_path) {
    rom_path_ = rom_path;

    std::ifstream f(rom_path, std::ios::binary);
    if (!f.is_open()) {
        return false;
    }

    f.read(reinterpret_cast<char*>(&header_), sizeof(RomHeader));
    if (!f) {
        return false;
    }

    f.close();

    uint32_t magic_u32;
    std::memcpy(&magic_u32, header_.magic, 4);
    if (magic_u32 != RomHeader::EXPECTED_MAGIC) {
        return false;
    }

    return true;
}

bool RomReader::parse() {
    std::ifstream f(rom_path_, std::ios::binary);
    if (!f.is_open()) {
        return false;
    }

    f.read(reinterpret_cast<char*>(&header_), sizeof(RomHeader));
    if (!f) {
        f.close();
        return false;
    }

    uint32_t magic_u32;
    std::memcpy(&magic_u32, header_.magic, 4);
    if (magic_u32 != RomHeader::EXPECTED_MAGIC) {
        f.close();
        return false;
    }

    index_data_.resize(header_.index_length);
    f.seekg(INDEX_OFFSET, std::ios::beg);
    f.read(reinterpret_cast<char*>(index_data_.data()), header_.index_length);
    if (!f) {
        f.close();
        return false;
    }

    f.close();

    root_entry_ = std::make_unique<RomEntry>();
    root_entry_->name = "/";
    root_entry_->is_directory = true;
    root_entry_->data_offset = 0;
    root_entry_->data_size = 0;

    parse_directory_recursive(*root_entry_, INDEX_OFFSET, INDEX_OFFSET, 0);

    count_entries(*root_entry_);

    return true;
}

void RomReader::parse_directory_recursive(
    RomEntry& dir_entry,
    uint32_t dir_file_offset,
    uint32_t current_dir_offset,
    int depth) {

    if (depth > 10) {
        return;
    }

    uint32_t rel_dir_offset = dir_file_offset - INDEX_OFFSET;
    if (rel_dir_offset + 4 > index_data_.size()) {
        return;
    }

    // First 4 bytes = entry_count
    uint32_t entry_count;
    std::memcpy(&entry_count, index_data_.data() + rel_dir_offset, 4);

    const uint8_t* ptr = index_data_.data() + rel_dir_offset + 4;
    const uint8_t* end = index_data_.data() + index_data_.size();

    uint32_t entries_remaining = entry_count;
    while (ptr + 12 <= end && entries_remaining > 0) {
        // Parse entry (12 bytes: name_off_flags, data_offset, data_size)
        uint32_t name_offset_and_flag;
        std::memcpy(&name_offset_and_flag, ptr, 4);
        ptr += 4;

        uint32_t data_offset;
        std::memcpy(&data_offset, ptr, 4);
        ptr += 4;

        uint32_t data_size;
        std::memcpy(&data_size, ptr, 4);
        ptr += 4;

        bool is_dir = (name_offset_and_flag & 0x80000000) != 0;
        uint32_t name_offset = name_offset_and_flag & 0x7FFFFFFF;

        // Read name (relative to current_dir_offset)
        uint32_t rel_name_offset = current_dir_offset - INDEX_OFFSET + name_offset;
        std::string name;
        if (rel_name_offset < index_data_.size()) {
            const char* name_str = reinterpret_cast<const char*>(
                index_data_.data() + rel_name_offset);
            name = name_str;
        }

        // End marker or skip . and ..
        if (name.empty() || name[0] == '\0' || name == "." || name == "..") {
            entries_remaining--;
            continue;
        }

        RomEntry entry;
        entry.name = name;
        entry.data_offset = data_offset;
        entry.data_size = data_size;
        entry.is_directory = is_dir;
        entry._entries_offset = data_offset;

        if (is_dir) {
            uint32_t sub_file_offset = data_offset * DIRECTORY_OFFSET_MULTIPLIER + INDEX_OFFSET;
            if (sub_file_offset - INDEX_OFFSET < index_data_.size()) {
                parse_directory_recursive(entry, sub_file_offset, sub_file_offset, depth + 1);
            }
        }

        dir_entry.children.push_back(std::move(entry));
        entries_remaining--;
    }
}

RomEntry RomReader::parse_entry_at(const uint8_t* ptr, uint32_t current_dir_offset) const {
    RomEntry entry;

    uint32_t name_offset_and_flag;
    std::memcpy(&name_offset_and_flag, ptr, 4);
    ptr += 4;

    uint32_t data_offset;
    std::memcpy(&data_offset, ptr, 4);
    ptr += 4;

    uint32_t data_size;
    std::memcpy(&data_size, ptr, 4);

    bool is_dir = (name_offset_and_flag & 0x80000000) != 0;
    uint32_t name_offset = name_offset_and_flag & 0x7FFFFFFF;

    uint32_t rel_name_offset = current_dir_offset - INDEX_OFFSET + name_offset;
    if (rel_name_offset < index_data_.size()) {
        const char* name_str = reinterpret_cast<const char*>(index_data_.data() + rel_name_offset);
        entry.name = name_str;
    } else {
        entry.name = "";
    }

    entry.data_offset = data_offset;
    entry.data_size = data_size;
    entry.is_directory = is_dir;
    entry._entries_offset = data_offset;

    return entry;
}

RomEntry RomReader::parse_directory(uint32_t dir_file_offset, uint32_t current_dir_offset) {
    RomEntry dir_entry;
    dir_entry.name = "/";
    dir_entry.is_directory = true;
    dir_entry.data_offset = 0;
    dir_entry.data_size = 0;

    uint32_t rel_dir_offset = dir_file_offset - INDEX_OFFSET;
    if (rel_dir_offset + 4 > index_data_.size()) {
        return dir_entry;
    }

    uint32_t entry_count;
    std::memcpy(&entry_count, index_data_.data() + rel_dir_offset, 4);

    const uint8_t* ptr = index_data_.data() + rel_dir_offset + 4;
    const uint8_t* end = index_data_.data() + index_data_.size();

    uint32_t entries_remaining = entry_count;
    while (ptr + 12 <= end && entries_remaining > 0) {
        RomEntry entry = parse_entry_at(ptr, current_dir_offset);

        if (entry.name.empty() || entry.name[0] == '\0' || entry.name == "." || entry.name == "..") {
            ptr += 12;
            entries_remaining--;
            continue;
        }

        if (entry.is_directory) {
            uint32_t sub_file_offset = entry.data_offset * DIRECTORY_OFFSET_MULTIPLIER + INDEX_OFFSET;
            if (sub_file_offset - INDEX_OFFSET < index_data_.size()) {
                entry.children = parse_directory(sub_file_offset, current_dir_offset).children;
            }
        }

        dir_entry.children.push_back(std::move(entry));
        ptr += 12;
        entries_remaining--;
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
