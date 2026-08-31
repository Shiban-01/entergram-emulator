#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <atomic>

namespace entergram {

// Audio playback system for .nxa files (Entergram's proprietary audio format).
// Uses libopus to decode NXA1 files, SDL_Audio for playback.
class AudioPlayer {
public:
    AudioPlayer();
    ~AudioPlayer();

    AudioPlayer(const AudioPlayer&) = delete;
    AudioPlayer& operator=(const AudioPlayer&) = delete;

    // Initialize SDL audio subsystem
    bool initialize();

    // Shutdown audio subsystem
    void shutdown();

    // Load and start playing audio data (already decoded PCM)
    void play_pcm(const std::vector<int16_t>& samples, int sample_rate, int channels);

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

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace entergram
