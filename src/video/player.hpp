#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <optional>

namespace entergram {

struct RgbaFrame {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> data;  // width * height * 4 bytes
    bool valid() const { return !data.empty() && width > 0 && height > 0; }
};

class VideoPlayer {
public:
    VideoPlayer();
    ~VideoPlayer();

    VideoPlayer(const VideoPlayer&) = delete;
    VideoPlayer& operator=(const VideoPlayer&) = delete;

    bool open(const std::string& file_path);
    void close();

    std::optional<RgbaFrame> read_frame();
    bool is_eof() const;
    bool has_pending_frame() const;
    void mark_frame_consumed();

    int width() const;
    int height() const;
    double frame_rate() const;
    double duration_seconds() const;
    int64_t total_frames() const;
    int frames_decoded_count() const;
    int64_t nb_frames() const;

    void pause();
    void resume();
    void seek(double timestamp_seconds);

    // Audio extraction from video
    bool has_audio_stream() const;
    // Extract audio, optionally limited to max_duration_seconds (0 = no limit)
    std::vector<int16_t> extract_audio(double target_sample_rate = 48000, int target_channels = 1, double max_duration_seconds = 0.0);
    int audio_sample_rate() const;
    std::string last_error() const { return last_error_; }
    bool has_error() const { return !last_error_.empty(); }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::string last_error_;
};

} // namespace entergram
