#include "audio/nxa_decoder.hpp"
#include <cstdio>
#include <fstream>
#include <cassert>

using namespace entergram;

int main() {
    printf("=== NXA Decoder Test ===\n\n");

    // Load an NXA file from the decompiled assets
    std::string nxa_path = "C:/Users/francisco.q/Desktop/Umineko-Shin/assets/data/voice/00/52100342_o.nxa";

    std::ifstream f(nxa_path, std::ios::binary | std::ios::ate);
    if (!f) {
        printf("ERROR: Cannot open %s\n", nxa_path.c_str());
        return 1;
    }

    size_t size = f.tellg();
    f.seekg(0);
    std::vector<uint8_t> data(size);
    f.read(reinterpret_cast<char*>(data.data()), size);
    f.close();

    printf("File: %s (%zu bytes)\n", nxa_path.c_str(), size);

    // Parse header
    auto info = parse_nxa(data);
    if (!info) {
        printf("ERROR: Invalid NXA1 format\n");
        return 1;
    }

    printf("NXA1 Info:\n");
    printf("  Version: %u\n", info->version);
    printf("  File size: %u\n", info->file_size);
    printf("  Sample rate: %u Hz\n", info->sample_rate);
    printf("  Channels: %u\n", info->channel_count);
    printf("  Frame size: %u bytes\n", info->frame_size);
    printf("  Frame samples: %u\n", info->frame_samples);
    printf("  Pre-skip: %u\n", info->pre_skip);
    printf("  Num samples: %u\n", info->num_samples);
    printf("  Loop start: %u\n", info->loop_start);
    printf("  Loop end: %u\n", info->loop_end);
    printf("  Audio data offset: 0x%X\n", info->audio_data_offset);
    printf("  Duration: %.2f seconds\n", info->duration_seconds());

    // Decode
    printf("\nDecoding...\n");
    NxaDecoder decoder;
    std::vector<int16_t> pcm = decoder.decode(data);

    if (pcm.empty()) {
        printf("ERROR: Decoding failed: %s\n", decoder.last_error().c_str());
        return 1;
    }

    printf("Decoded: %zu samples (%zu bytes)\n",
           pcm.size() / info->channel_count, pcm.size() * sizeof(int16_t));
    printf("Expected samples: %u\n", info->num_samples - info->pre_skip);
    printf("Duration: %.2f seconds\n",
           (double)(pcm.size() / info->channel_count) / info->sample_rate);

    // Write to WAV for verification
    std::string wav_path = "build/test_audio_output.wav";
    // WAV header
    uint32_t sample_count = pcm.size() / info->channel_count;
    uint32_t byte_rate = info->sample_rate * info->channel_count * 2;
    uint32_t data_size = pcm.size() * sizeof(int16_t);

    std::ofstream wav(wav_path, std::ios::binary);
    // RIFF header
    wav.write("RIFF", 4);
    uint32_t chunk_size = 36 + data_size;
    wav.write(reinterpret_cast<const char*>(&chunk_size), 4);
    wav.write("WAVE", 4);
    // fmt chunk
    wav.write("fmt ", 4);
    uint32_t fmt_chunk_size = 16;
    wav.write(reinterpret_cast<const char*>(&fmt_chunk_size), 4);
    uint16_t audio_format = 1;  // PCM
    wav.write(reinterpret_cast<const char*>(&audio_format), 2);
    wav.write(reinterpret_cast<const char*>(&info->channel_count), 2);
    wav.write(reinterpret_cast<const char*>(&info->sample_rate), 4);
    wav.write(reinterpret_cast<const char*>(&byte_rate), 4);
    uint16_t block_align = info->channel_count * 2;
    wav.write(reinterpret_cast<const char*>(&block_align), 2);
    uint16_t bits_per_sample = 16;
    wav.write(reinterpret_cast<const char*>(&bits_per_sample), 2);
    // data chunk
    wav.write("data", 4);
    wav.write(reinterpret_cast<const char*>(&data_size), 4);
    wav.write(reinterpret_cast<const char*>(pcm.data()), data_size);
    wav.close();

    printf("\nWAV written: %s\n", wav_path.c_str());
    printf("PASS: NXA decoder\n");
    return 0;
}
