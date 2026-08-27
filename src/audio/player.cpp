#include "player.hpp"

// Audio player implementation for Entergram emulator
// Uses FFmpeg (libavformat + libswresample) for decoding .nxa files
// and SDL_Audio for playback.

#ifdef _WIN32
    // Windows-specific includes will be added when SDL2 is linked
#endif

namespace entergram {

struct AudioPlayer::Impl {
    bool initialized = false;
    bool playing = false;
    float volume = 1.0f;
    double duration = 0.0;
    double position = 0.0;
};

AudioPlayer::AudioPlayer() : impl_(std::make_unique<Impl>()) {}
AudioPlayer::~AudioPlayer() = default;

bool AudioPlayer::initialize() {
    // SDL audio will be initialized when the main window is created
    impl_->initialized = true;
    return true;
}

void AudioPlayer::shutdown() {
    stop();
    impl_->initialized = false;
}

bool AudioPlayer::play(const std::string& file_path) {
    // TODO: Use FFmpeg to decode .nxa file
    // For now, mark as playing
    impl_->playing = true;
    return true;
}

void AudioPlayer::stop() {
    impl_->playing = false;
}

void AudioPlayer::pause(bool paused) {
    impl_->playing = !paused;
}

void AudioPlayer::set_volume(float volume) {
    impl_->volume = std::max(0.0f, std::min(1.0f, volume));
}

bool AudioPlayer::is_playing() const {
    return impl_->playing;
}

double AudioPlayer::position() const {
    return impl_->position;
}

void AudioPlayer::wait_until_done() {
    while (impl_->playing) {
        // TODO: implement proper waiting with SDL audio callbacks
    }
}

} // namespace entergram
