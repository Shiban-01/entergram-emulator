/**
 * Entergram Emulator - Main Entry Point
 *
 * Cross-platform emulator for the Entergram visual novel engine.
 * Supports Umineko When They Cry and Higurashi When They Cry.
 *
 * Uses C++20, SDL2 for window/input/audio, OpenGL 3.3 for rendering,
 * and FFmpeg libraries for video playback.
 */

#include "core/rom_reader.hpp"
#include "core/snr_vm.hpp"
#include "core/snr0_parser.hpp"
#include "video/player.hpp"
#include "renderer/renderer.hpp"
#include "renderer/layer_manager.hpp"
#include "audio/player.hpp"
#include "input/manager.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <chrono>

// Game state enum
enum class GameState {
    Boot,
    IntroVideo,
    MainMenu,      // SELECT menu appears after intro video completes
    Novel,
    ChoiceMenu,
    Shutdown
};

// Callback handler — bridges SNR VM to game engine subsystems
class GameCallbacks : public entergram::SnrVmCallbacks {
public:
    class GameEngine* engine = nullptr;

    void on_voice_play(const std::string& file_name, int volume, int flags) override {
        if (engine) engine->play_voice(file_name, volume);
    }
    void on_voice_stop(int voice_id) override {
        if (engine) engine->stop_voice(voice_id);
    }
    void on_voice_wait(int voice_id) override {
        if (engine) engine->wait_for_voice(voice_id);
    }
    void on_bgm_play(const std::string& file_name, int volume) override {
        if (engine) engine->play_bgm(file_name, volume);
    }
    void on_bgm_stop() override {
        if (engine) engine->stop_bgm();
    }
    void on_se_play(const std::string& file_name, int volume) override {
        if (engine) engine->play_se(file_name, volume);
    }
    void on_movie_play(const std::string& file_name) override {
        if (engine) engine->play_movie(file_name);
    }
    void on_text_display(const std::string& text, const std::string& character_name) override {
        if (engine) engine->display_text(text, character_name);
    }
    void on_wait(int frames) override {
        if (engine) engine->wait_frames(frames);
    }
};

// Game engine — coordinates all subsystems
class GameEngine {
public:
    GameEngine() : state_(GameState::Boot) {}

    bool initialize(const std::string& rom_path) {
        std::cout << "=== Entergram Emulator v0.1.0 ===\n";
        std::cout << "Loading ROM: " << rom_path << "\n";

        if (!rom_.open(rom_path)) {
            std::cerr << "ERROR: Cannot open ROM file\n";
            return false;
        }

        if (!rom_.parse()) {
            std::cerr << "ERROR: Failed to parse ROM2 format\n";
            return false;
        }

        std::cout << "ROM loaded: " << rom_.total_file_count() << " files, "
                  << rom_.total_directory_count() << " directories\n";

        // List root directory contents
        const auto& root = rom_.root();
        std::cout << "Root directory entries:\n";
        for (const auto& entry : root.children) {
            std::cout << "  " << entry.name << " ("
                      << (entry.is_directory ? "dir" : "file") << ")\n";
        }

        // Load main script
        if (!load_script("main.snr")) {
            std::cerr << "WARNING: Cannot load main.snr\n";
        }

        state_ = GameState::IntroVideo;
        callbacks_.engine = this;
        return true;
    }

    // Main loop tick — returns false to exit
    bool run_frame() {
        switch (state_) {
            case GameState::Boot:
                state_ = GameState::IntroVideo;
                break;

            case GameState::IntroVideo:
                if (vm_.is_running()) {
                    // VM handles MOVIE command which starts video playback
                    vm_.run(&callbacks_);
                } else {
                    state_ = GameState::MainMenu;
                }
                break;

            case GameState::MainMenu:
                // SELECT menu — wait for user input
                break;

            case GameState::Novel:
                if (wait_frames_remaining_ > 0) {
                    wait_frames_remaining_--;
                } else if (vm_.is_running()) {
                    vm_.step(&callbacks_);
                }
                break;

            case GameState::Shutdown:
                return false;
        }
        return true;
    }

    // Audio
    void play_voice(const std::string& file_name, int volume) {
        printf("VOICE: %s (vol=%d)\n", file_name.c_str(), volume);
    }
    void stop_voice(int voice_id) {}
    void wait_for_voice(int voice_id) {}
    void play_bgm(const std::string& file_name, int volume) {
        printf("BGM: %s (vol=%d)\n", file_name.c_str(), volume);
    }
    void stop_bgm() {}
    void play_se(const std::string& file_name, int volume) {
        printf("SE: %s (vol=%d)\n", file_name.c_str(), volume);
    }

    // Video
    void play_movie(const std::string& file_name) {
        printf("MOVIE: %s\n", file_name.c_str());
        // Try to extract from ROM
        auto movie = rom_.extract_file("movie/" + file_name);
        if (movie) {
            // Write to temp file
            std::string temp_path = "/tmp/" + file_name;
            std::ofstream f(temp_path, std::ios::binary);
            f.write(reinterpret_cast<const char*>(movie->data.data()),
                    movie->data.size());
            f.close();

            if (video_player_.open(temp_path)) {
                printf("Movie opened: %dx%d @ %.1ffps, %zu bytes\n",
                       video_player_.width(), video_player_.height(),
                       video_player_.frame_rate(), movie->data.size());
            } else {
                printf("Failed to open movie: %s\n",
                       video_player_.last_error().c_str());
            }
        } else {
            printf("Movie not found in ROM\n");
        }
    }

    // Text
    void display_text(const std::string& text, const std::string& character_name) {
        printf("[TEXT] %s: %s\n", character_name.c_str(), text.c_str());
    }
    void wait_frames(int frames) {
        wait_frames_remaining_ = frames;
    }

    GameState state() const { return state_; }

    entergram::RomReader& rom() { return rom_; }
    entergram::SnrVm& vm() { return vm_; }

private:
    bool load_script(const std::string& script_path) {
        auto file = rom_.extract_file(script_path);
        if (!file) {
            return false;
        }

        if (entergram::Snr0Parser::is_valid_snr0(file->data)) {
            current_script_ = entergram::Snr0Parser::parse(file->data);
        } else {
            current_script_ = file->data;
        }

        vm_.load_script(current_script_);
        return true;
    }

private:
    GameState state_;
    entergram::RomReader rom_;
    entergram::SnrVm vm_;
    GameCallbacks callbacks_;
    entergram::VideoPlayer video_player_;
    std::vector<uint8_t> current_script_;
    int wait_frames_remaining_ = 0;
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <path_to_data.rom>\n";
        return 1;
    }

    GameEngine engine;
    if (!engine.initialize(argv[1])) {
        std::cerr << "Failed to initialize emulator\n";
        return 1;
    }

    // Main loop
    while (engine.run_frame()) {
        // In a full implementation, this would:
        // 1. Poll SDL events (input)
        // 2. Decode video frames if playing a movie
        // 3. Render layers via OpenGL
        // 4. Play/update audio
    }

    std::cout << "Emulator shut down\n";
    return 0;
}
