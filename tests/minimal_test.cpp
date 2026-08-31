#include "../src/core/rom_reader.hpp"
#include <cstdio>
#include <cstdint>

using namespace entergram;

int main() {
    printf("=== Minimal ROM Reader Test ===\n\n");
    
    RomReader reader;
    std::string rom_path = "C:/Users/francisco.q/AppData/Roaming/eden/dump/01006A300BA2C000/romfs/data.rom";
    
    printf("Opening ROM...\n");
    fflush(stdout);
    bool opened = reader.open(rom_path);
    printf("open() returned: %d\n", opened);
    fflush(stdout);
    
    if (!opened) return 1;
    
    printf("Parsing ROM...\n");
    fflush(stdout);
    bool parsed = reader.parse();
    printf("parse() returned: %d\n", parsed);
    fflush(stdout);
    
    if (!parsed) return 1;
    
    auto& hdr = reader.header();
    printf("Magic: %c%c%c%c\n", hdr.magic[0], hdr.magic[1], hdr.magic[2], hdr.magic[3]);
    printf("Version: 0x%08X\n", hdr.version);
    printf("Index length: %u\n", hdr.index_length);
    printf("Offset multiplier: %u\n", hdr.offset_multiplier);
    printf("File count: %zu\n", reader.total_file_count());
    printf("Dir count: %zu\n", reader.total_directory_count());
    fflush(stdout);
    
    // List root
    const auto& root = reader.root();
    printf("\nRoot entries (%zu):\n", root.children.size());
    for (const auto& e : root.children) {
        printf("  %s (%s)\n", e.name.c_str(), e.is_directory ? "dir" : "file");
    }
    fflush(stdout);
    
    // List movie directory
    for (const auto& e : root.children) {
        if (e.name == "movie" && e.is_directory) {
            printf("\nMovie entries (%zu):\n", e.children.size());
            for (const auto& m : e.children) {
                printf("  %s (%u bytes)\n", m.name.c_str(), m.data_size);
            }
        }
        if (e.name == "bgm" && e.is_directory) {
            printf("\nBGM entries (%zu):\n", e.children.size());
            for (size_t i = 0; i < std::min(size_t(5), e.children.size()); i++) {
                printf("  %s (%u bytes)\n", e.children[i].name.c_str(), e.children[i].data_size);
            }
        }
    }
    fflush(stdout);
    
    return 0;
}
