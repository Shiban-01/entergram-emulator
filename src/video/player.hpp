#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace entergram {

// RGBA frame (8-bit per channel, no stride alignment needed for OpenGL)
struct RgbaFrame {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> data;  // width * height * 4 bytes

    bool valid() const { return !data.empty() && width > 0 && height > 0; }
};

// Video player using FFmpeg libraries (libavformat + libavcodec + libswscale)
// Decodes H.264 video from .mp4 containers and provides RGBA frames for rendering.
//
// Usage pattern:
//   VideoPlayer player;
//   if (player.open("movie.mp4")) {
//       while (!player.is_eof() || player.has_pending_frame()) {
//           if (auto frame = player.read_frame()) {
//               render_frame(*frame);
//               player.mark_frame_consumed();
//           }
//           std::this_thread::sleep_for(...);
//       }
//   }
class VideoPlayer {
public:
    VideoPlayer();
    ~VideoPlayer();

    VideoPlayer(const VideoPlayer&) = delete;
    VideoPlayer& operator=(const VideoPlayer&) = delete;

    // Open a video file for playback
    bool open(const std::string& file_path);

    // Close the video file and free resources
    void close();

    // Read the next decoded frame as RGBA
    // Returns empty RgbaFrame if no frame available (call again)
    // Returns nullopt on EOF or error
    std::optional<RgbaFrame> read_frame();

    // Check if we've reached EOF
    bool is_eof() const;

    // Check if there's a decoded frame waiting to be consumed
    bool has_pending_frame() const;

    // Mark the current frame as consumed (advances internal buffer)
    void mark_frame_consumed();

    // Video properties
    int width() const;
    int height() const;
    double frame_rate() const;  // fps

    // Playback control
    void pause();
    void resume();
    void seek(double timestamp_seconds);  // Seek to timestamp

    // Error handling
    bool has_error() const { return !last_error_.empty(); }
    std::string last_error() const { return last_error_; }

private:
    // Opaque implementation to avoid exposing FFmpeg types in header
    struct Impl;
    std::unique_ptr<Impl> impl_;

    std::string last_error_;
};

} // namespace entergram
