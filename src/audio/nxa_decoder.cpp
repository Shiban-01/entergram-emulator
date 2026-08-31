#include "nxa_decoder.hpp"

#include <cstring>
#include <cstdio>
#include <algorithm>
#include <string>

namespace entergram {

// =============================================================================
// NXA1 Format Header:
//   0x00: magic "NXA1" (4 bytes)
//   0x04: version (u32) = 2
//   0x08: file_size (u32)
//   0x0C: AudioInfo (26 bytes):
//     0x0C:  sample_rate   (u32)
//     0x10:  channel_count (u16)
//     0x12:  frame_size    (u16) - bytes per opus frame
//     0x14:  frame_samples (u16) - samples per opus frame
//     0x16:  pre_skip      (u16)
//     0x18:  num_samples   (u32)
//     0x1C:  loop_start    (u32)
//     0x20:  loop_end      (u32)
//   0x26: end of AudioInfo (align to 0x30, 10 bytes padding)
//   0x30: raw opus frame data
// =============================================================================

static constexpr uint32_t NXA_AUDIO_DATA_OFFSET = 0x30;

std::optional<NxaInfo> parse_nxa(const std::vector<uint8_t>& file_data) {
    if (file_data.size() < NXA_AUDIO_DATA_OFFSET) {
        return std::nullopt;
    }

    if (std::memcmp(file_data.data(), "NXA1", 4) != 0) {
        return std::nullopt;
    }

    NxaInfo info;

    std::memcpy(&info.version, file_data.data() + 4, 4);
    std::memcpy(&info.file_size, file_data.data() + 8, 4);

    if (info.version != 2) {
        return std::nullopt;
    }

    std::memcpy(&info.sample_rate, file_data.data() + 0x0C, 4);
    std::memcpy(&info.channel_count, file_data.data() + 0x10, 2);
    std::memcpy(&info.frame_size, file_data.data() + 0x12, 2);
    std::memcpy(&info.frame_samples, file_data.data() + 0x14, 2);
    std::memcpy(&info.pre_skip, file_data.data() + 0x16, 2);
    std::memcpy(&info.num_samples, file_data.data() + 0x18, 4);
    std::memcpy(&info.loop_start, file_data.data() + 0x1C, 4);
    std::memcpy(&info.loop_end, file_data.data() + 0x20, 4);

    if (info.sample_rate == 0 || info.frame_size == 0 || info.num_samples == 0) {
        return std::nullopt;
    }

    info.audio_data_offset = NXA_AUDIO_DATA_OFFSET;

    return info;
}

// =============================================================================
// NXA Decoder (using libopus directly)
// =============================================================================

NxaDecoder::NxaDecoder() {
    int err;
    decoder_ = opus_decoder_create(sample_rate_, channel_count_, &err);
    if (err != OPUS_OK) {
        last_error_ = "Failed to create Opus decoder: " + std::to_string(err);
        decoder_ = nullptr;
    }
}

NxaDecoder::~NxaDecoder() {
    if (decoder_) {
        opus_decoder_destroy(decoder_);
        decoder_ = nullptr;
    }
}

std::vector<int16_t> NxaDecoder::decode(const std::vector<uint8_t>& file_data) {
    if (!decoder_) {
        last_error_ = "Opus decoder not initialized";
        return {};
    }

    auto info = parse_nxa(file_data);
    if (!info || !info->valid()) {
        last_error_ = "Invalid NXA1 format";
        return {};
    }

    // Reset decoder state
    int err = opus_decoder_ctl(decoder_, OPUS_RESET_STATE);
    if (err != OPUS_OK) {
        last_error_ = "Failed to reset Opus decoder";
        return {};
    }

    // Reconfigure for the file's channel count (opus_decoder_ctl OPUS_SET_CHANNELS)
    if (info->channel_count != channel_count_) {
        channel_count_ = info->channel_count;
        // Recreate decoder with correct channel count
        opus_decoder_destroy(decoder_);
        decoder_ = opus_decoder_create(48000, channel_count_, &err);
        if (err != OPUS_OK) {
            last_error_ = "Failed to recreate Opus decoder";
            return {};
        }
    }

    pre_skip_ = info->pre_skip;

    // Decode opus frames
    std::vector<int16_t> all_samples;
    uint32_t audio_offset = info->audio_data_offset;
    uint32_t data_size = static_cast<uint32_t>(file_data.size());

    int frame_count = 0;

    // Opus max frame size: 2880 samples per channel at 48kHz for 20ms frames
    // For 60ms frames it can be up to 2880*3 = 8640 samples
    // Buffer: 12732 samples per channel (libopus recommended max)
    std::vector<opus_int16> output_buffer(12732 * info->channel_count);

    while (audio_offset + info->frame_size <= data_size) {
        int decoded_samples = opus_decode(
            decoder_,
            file_data.data() + audio_offset,
            info->frame_size,
            output_buffer.data(),
            static_cast<int>(output_buffer.size() / info->channel_count),
            0  // decode_fec: 0 = no forward error correction
        );

        if (decoded_samples < 0) {
            char err_buf[256];
            snprintf(err_buf, sizeof(err_buf), "Opus decode error: %s (frame %d)",
                     opus_strerror(decoded_samples), frame_count);
            last_error_ = err_buf;
            break;
        }

        if (decoded_samples > 0) {
            all_samples.insert(all_samples.end(),
                output_buffer.begin(),
                output_buffer.begin() + decoded_samples * info->channel_count);
        }

        audio_offset += info->frame_size;
        frame_count++;

        if (frame_count > 100000) break;
    }

    printf("  NXA decoded: %d frames, %zu total samples\n",
           frame_count, all_samples.size() / info->channel_count);

    // Apply pre-skip: skip first pre_skip samples
    if (info->pre_skip > 0 && all_samples.size() >= info->pre_skip * info->channel_count) {
        all_samples.erase(all_samples.begin(),
            all_samples.begin() + info->pre_skip * info->channel_count);
        printf("  Applied pre-skip: skipped %u samples\n", info->pre_skip);
    }

    return all_samples;
}

} // namespace entergram
