#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <memory>

// Opus includes
#include <opus/opus.h>

namespace entergram {

// NXA audio format header info (parsed from NXA1 files)
struct NxaInfo {
    uint32_t version = 0;
    uint32_t file_size = 0;
    uint32_t sample_rate = 0;
    uint16_t channel_count = 0;
    uint16_t frame_size = 0;
    uint16_t frame_samples = 0;
    uint16_t pre_skip = 0;
    uint32_t num_samples = 0;
    uint32_t loop_start = 0;
    uint32_t loop_end = 0;

    uint32_t audio_data_offset = 0;

    bool valid() const { return sample_rate > 0 && file_size > 0; }
    double duration_seconds() const { return sample_rate > 0 ? static_cast<double>(num_samples) / sample_rate : 0.0; }
};

std::optional<NxaInfo> parse_nxa(const std::vector<uint8_t>& file_data);

class NxaDecoder {
public:
    NxaDecoder();
    ~NxaDecoder();

    NxaDecoder(const NxaDecoder&) = delete;
    NxaDecoder& operator=(const NxaDecoder&) = delete;

    std::vector<int16_t> decode(const std::vector<uint8_t>& file_data);

    bool has_error() const { return !last_error_.empty(); }
    const std::string& last_error() const { return last_error_; }

private:
    std::string last_error_;
    OpusDecoder* decoder_ = nullptr;
    opus_int32 sample_rate_ = 48000;
    int channel_count_ = 1;
    opus_int32 pre_skip_ = 0;
};

} // namespace entergram
