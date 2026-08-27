/**
 * Entergram Emulator - Main Entry Point
 *
 * Cross-platform emulator for the Entergram visual novel engine.
 * Supports Umineko When They Cry and Higurashi When They Cry.
 *
 * Uses C++20, SDL2 for window/input/audio, OpenGL 3.3 for rendering,
 * and FFmpeg libraries for video playback.
 *
 * Build: See CMakeLists.txt
 */

#include "core/rom_reader.hpp"
#include "core/snr_vm.hpp"
#include "core/snr0_parser.hpp"
#include "video/player.hpp"
#include "renderer/renderer.hpp"
#include "audio/player.hpp"
#include "input/manager.hpp"

#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
    #include <windows.h>
#endif

// Game state enum
enum class GameState {
    Boot,
    IntroVideo,
    MainMenu,      // SELECT menu
    Novel,         // Gameplay
    ChoiceMenu,
    Shutdown
};

// Forward declarations
class GameCallbacks;
class GameEngine;

// Callback handler for SNR VM — bridges VM commands to actual game subsystems
class GameCallbacks : public entergram::SnrVmCallbacks {
public:
    GameEngine* engine;

    // Audio callbacks
    void on_voice_play(const std::string& file_name, int volume, int flags) override {
        if (engine) engine->play_voice(file_name, volume);
    }

    void on_voice_stop(int voice_id) override {
        if (engine) engine->stop_voice(voice_id);
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

    // Video callbacks
    void on_movie_play(const std::string& file_name) override {
        if (engine) engine->play_movie(file_name);
    }

    // Text callbacks
    void on_text_display(const std::string& text, const std::string& character_name) override {
        if (engine) engine->display_text(text, character_name);
    }

    void on_wait(int frames) override {
        if (engine) engine->wait_frames(frames);
    }
};

// Game engine — coordinates ROM access, VM, renderer, and audio
class GameEngine {
public:
    GameEngine() : state_(GameState::Boot), vm_(), callbacks_() {}

    bool initialize(const std::string& rom_path);

    // Main loop tick — returns false to exit
    bool run_frame();

    // Audio
    void play_voice(const std::string& file_name, int volume);
    void stop_voice(int voice_id);
    void play_bgm(const std::string& file_name, int volume);
    void stop_bgm();
    void play_se(const std::string& file_name, int volume);

    // Video
    void play_movie(const std::string& file_name);

    // Text
    void display_text(const std::string& text, const std::string& character_name);
    void wait_frames(int frames);

    // State
    GameState state() const { return state_; }
    void set_state(GameState s) { state_ = s; }

    entergram::RomReader& rom() { return rom_; }
    entergram::SnrVm& vm() { return vm_; }

private:
    GameState state_;
    entergram::RomReader rom_;
    entergram::SnrVm vm_;
    GameCallbacks callbacks_;
    entergram::VideoPlayer video_player_;

    // Current script bytecode
    std::vector<uint8_t> current_script_;

    // Wait counter
    int wait_frames_remaining_ = 0;

    // Load and run a script file from ROM
    bool load_script(const std::string& script_path);
};

bool GameEngine::initialize(const std::string& rom_path) {
    std::cout << "Entergram Emulator v0.1.0\n";
    std::cout << "Initializing...\n";

    if (!rom_.open(rom_path)) {
        std::cerr << "ERROR: Cannot open ROM file: " << rom_path << "\n";
        return false;
    }

    if (!rom_.parse()) {
        std::cerr << "ERROR: Failed to parse ROM2 format\n";
        return false;
    }

    std::cout << "ROM loaded: " << rom_.total_file_count() << " files, "
              << rom_.total_directory_count() << " directories\n";

    // Load the main script (main.snr)
    if (!load_script("main.snr")) {
        std::cerr << "ERROR: Cannot load main.snr\n";
        return false;
    }

    // Wire callbacks
    callbacks_.engine = this;
    state_ = GameState::IntroVideo;

    return true;
}

bool GameEngine::load_script(const std::string& script_path) {
    auto file = rom_.extract_file(script_path);
    if (!file) {
        std::cerr << "ERROR: Cannot extract " << script_path << " from ROM\n";
        return false;
    }

    // Check if this is an SNR0 file
    if (entergram::Snr0Parser::is_valid_snr0(file->data)) {
        current_script_ = entergram::Snr0Parser::parse(file->data);
        if (current_script_.empty()) {
            std::cerr << "ERROR: Failed to decompress " << script_path << "\n";
            return false;
        }
    } else {
        // Assume it's already raw bytecode
        current_script_ = std::move(file->data);
    }

    vm_.load_script(current_script_);
    return true;
}

bool GameEngine::run_frame() {
    switch (state_) {
        case GameState::Boot:
            state_ = GameState::IntroVideo;
            break;

        case GameState::IntroVideo:
            // Run VM — it will dispatch MOVIE command for intro video
            if (vm_.is_running()) {
                vm_.run(&callbacks_);
            } else {
                state_ = GameState::MainMenu;
            }
            break;

        case GameState::MainMenu:
            // SELECT menu — wait for user input
            // The main loop handles input separately
            break;

        case GameState::Novel:
            // Running narrative
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

void GameEngine::play_movie(const std::string& file_name) {
    std::cout << "Playing movie: " << file_name << "\n";
    // Extract movie from ROM and play
    auto movie = rom_.extract_file("movie/" + file_name);
    if (movie) {
        // Write to temp file and open with VideoPlayer
        std::string temp_path = "/tmp/" + file_name;
        std::ofstream f(temp_path, std::ios::binary);
        f.write(reinterpret_cast<const char*>(movie->data.data()), movie->data.size());
        f.close();

        if (video_player_.open(temp_path)) {
            state_ = GameState::IntroVideo;
            // Video will play in the render loop
        }
    }
}

void GameEngine::play_voice(const std::string& file_name, int volume) {
    printf("Playing voice: %s (vol=%d)\n", file_name.c_str(), volume);
}

void GameEngine::stop_voice(int voice_id) {}

void GameEngine::play_bgm(const std::string& file_name, int volume) {
    printf("Playing BGM: %s (vol=%d)\n", file_name.c_str(), volume);
}

void GameEngine::stop_bgm() {}

void GameEngine::play_se(const std::string& file_name, int volume) {
    printf("Playing SE: %s (vol=%d)\n", file_name.c_str(), volume);
}

void GameEngine::display_text(const std::string& text, const std::string& character_name) {
    printf("[TEXT] %s: %s\n", character_name.c_str(), text.c_str());
}

void GameEngine::wait_frames(int frames) {
    wait_frames_remaining_ = frames;
}

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
        // Render frame (to be implemented with SDL2 + OpenGL)
        // Handle input
        // Step VM
    }

    std::cout << "Emulator shutting down\n";
    return 0;
}
