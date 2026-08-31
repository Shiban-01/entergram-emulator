#include "player.hpp"
#include "nxa_decoder.hpp"

#ifdef USE_SDL2
    #include <SDL2/SDL.h>
#endif

#include <cstring>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace entergram {

struct AudioPlayer::Impl {
    bool initialized = false;
    std::atomic<bool> playing{false};
    std::atomic<bool> paused{false};
    float volume = 1.0f;
    double position_seconds = 0.0;
    int sample_rate = 48000;
    int channels = 1;

    // Audio buffer for playback
    std::vector<int16_t> audio_buffer;
    size_t buffer_pos = 0;

    // SDL audio device
#ifdef USE_SDL2
    SDL_AudioDeviceID device = 0;
#endif
    std::mutex mutex;

    // Audio callback data
    static void SDLCALL audio_callback(void* userdata, uint8_t* stream, int len);

    ~Impl() {
        shutdown();
    }

    void shutdown() {
#ifdef USE_SDL2
        if (device) {
            SDL_CloseAudioDevice(device);
            device = 0;
        }
#endif
        initialized = false;
        playing = false;
        audio_buffer.clear();
        buffer_pos = 0;
    }
};

AudioPlayer::AudioPlayer() : impl_(std::make_unique<Impl>()) {}
AudioPlayer::~AudioPlayer() = default;

bool AudioPlayer::initialize() {
#ifdef USE_SDL2
    if (impl_->initialized) {
        return true;
    }

    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = 48000;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 4096;
    want.callback = Impl::audio_callback;
    want.userdata = impl_.get();

    impl_->device = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (impl_->device == 0) {
        return false;
    }

    impl_->sample_rate = have.freq;
    impl_->channels = have.channels;
    impl_->initialized = true;
#else
    impl_->initialized = true;
#endif
    return true;
}

void AudioPlayer::shutdown() {
    impl_->shutdown();
}

void AudioPlayer::Impl::audio_callback(void* userdata, uint8_t* stream, int len) {
    auto* impl = static_cast<Impl*>(userdata);
    if (!impl->playing || impl->paused) {
        SDL_memset(stream, 0, len);
        return;
    }

    std::lock_guard<std::mutex> lock(impl->mutex);
    size_t samples_needed = len / sizeof(int16_t);
    int16_t* out = reinterpret_cast<int16_t*>(stream);

    for (size_t i = 0; i < samples_needed; i++) {
        if (impl->buffer_pos < impl->audio_buffer.size()) {
            float vol = impl->volume;
            out[i] = static_cast<int16_t>(impl->audio_buffer[impl->buffer_pos++] * vol);
            impl->position_seconds += 1.0 / impl->sample_rate;
        } else {
            out[i] = 0;
            impl->playing = false;
        }
    }
}

void AudioPlayer::play_pcm(const std::vector<int16_t>& samples, int sample_rate, int channels) {
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->audio_buffer = samples;
        impl_->buffer_pos = 0;
        impl_->playing = true;
        impl_->paused = false;
        impl_->position_seconds = 0.0;
        impl_->sample_rate = sample_rate;
        impl_->channels = channels;
    }

#ifdef USE_SDL2
    if (impl_->device) {
        SDL_PauseAudioDevice(impl_->device, 0);
    }
#endif
}

void AudioPlayer::stop() {
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->playing = false;
        impl_->buffer_pos = 0;
        impl_->audio_buffer.clear();
    }
#ifdef USE_SDL2
    if (impl_->device) {
        SDL_PauseAudioDevice(impl_->device, 1);
    }
#endif
}

void AudioPlayer::pause(bool paused) {
    impl_->paused = paused;
}

void AudioPlayer::set_volume(float volume) {
    impl_->volume = std::max(0.0f, std::min(1.0f, volume));
}

bool AudioPlayer::is_playing() const {
    return impl_->playing;
}

double AudioPlayer::position() const {
    return impl_->position_seconds;
}

void AudioPlayer::wait_until_done() {
    while (is_playing()) {
#ifdef USE_SDL2
        SDL_Delay(10);
#else
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
#endif
    }
}

} // namespace entergram
