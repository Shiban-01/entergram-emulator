#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace entergram {

// Audio playback system for .nxa files (Entergram's proprietary audio format).
//
// The .nxa format wraps standard audio codecs (likely Ogg Vorbis or raw PCM)
// inside an Entergram container. This player extracts the audio data
// and plays it via SDL_Audio.
//
// For the initial implementation, we use FFmpeg (libavformat) to probe
// the .nxa files, since they may contain standard codec data.
class AudioPlayer {
public:
    AudioPlayer();
    ~AudioPlayer();

    // Load and start playing an audio file (.nxa or .ogg)
    bool play(const std::string& file_path);

    // Stop playback
    void stop();

    // Pause/unpause
    void pause(bool paused);

    // Set volume (0.0 to 1.0)
    void set_volume(float volume);

    // Check if currently playing
    bool is_playing() const;

    // Get playback position (in seconds)
    double position() const;

    // Wait for playback to complete (blocking)
    void wait_until_done();

    // Initialize SDL audio subsystem
    bool initialize();

    // Shutdown audio subsystem
    void shutdown();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace entergram
