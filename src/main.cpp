/**
 * Entergram Emulator - Main Entry Point
 *
 * Cross-platform emulator for the Entergram visual novel engine.
 * Uses C++20, SDL2 (window/input/audio), OpenGL 3.3 (rendering),
 * FFmpeg libraries (video/audio decoding).
 */

#include "core/rom_reader.hpp"
#include "core/snr_vm.hpp"
#include "core/snr0_parser.hpp"
#include "video/player.hpp"
#include "audio/nxa_decoder.hpp"
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
#include <thread>
#include <cstdio>
#ifdef _WIN32
    #include <windows.h>
    #include <io.h>
    #define isatty _isatty
    #define fileno _fileno
#endif
#include <filesystem>

#ifdef USE_SDL2
    #include <SDL2/SDL.h>
    #define WITH_SDL2 1
#else
    #define WITH_SDL2 0
#endif

// =============================================================================
// Configuration
// =============================================================================
constexpr int VIRTUAL_WIDTH = 1920;
constexpr int VIRTUAL_HEIGHT = 1080;

enum class GameState {
    RomSelect,
    Boot,
    IntroVideo,
    MainMenu,
    Novel,
    ChoiceMenu,
    Shutdown
};

// Forward declaration
class GameEngine;

// Callback handler for SNR VM -- bridges VM opcodes to engine methods
class GameCallbacks : public entergram::SnrVmCallbacks {
public:
    GameEngine* engine = nullptr;

    void on_voice_play(const std::string& file_name, int volume, int) override;
    void on_voice_stop(int voice_id) override;
    void on_voice_wait(int voice_id) override;
    void on_bgm_play(const std::string& file_name, int volume) override;
    void on_bgm_stop() override;
    void on_se_play(const std::string& file_name, int volume) override;
    void on_movie_play(const std::string& file_name) override;
    void on_movie_stop() override;
    void on_text_display(const std::string& text, const std::string& character_name) override;
    void on_choice(const std::vector<std::string>& options) override;
    void on_system_call(uint32_t code, const std::string& data) override;
    void on_wait(int frames) override;
};

// Game engine -- coordinates ROM, VM, renderer, video player, and audio
class GameEngine {
public:
    GameEngine();
    ~GameEngine() = default;

    bool initialize(const std::string& rom_path, bool list_mode = false);

#if WITH_SDL2
    int run_sdl();
#endif

    entergram::RomReader& rom() { return rom_; }
    entergram::SnrVm& vm() { return vm_; }

    GameState state() const { return state_; }
    void set_state(GameState s) { state_ = s; }

    // VM callbacks
    void play_voice(const std::string& file_name, int volume);
    void stop_voice(int voice_id);
    void wait_for_voice(int voice_id);
    void play_bgm(const std::string& file_name, int volume);
    void stop_bgm();
    void play_se(const std::string& file_name, int volume);
    void play_movie(const std::string& file_name);
    void display_text(const std::string& text, const std::string& character_name);
    void wait_frames(int frames);

    // Video
    bool extract_intro_video();
    void render_video_frame();
    void update_video();

private:
    bool load_script(const std::string& script_path);
    void handle_input();
    void render();

    GameState state_;
    entergram::RomReader rom_;
    entergram::SnrVm vm_;
    GameCallbacks callbacks_;
    entergram::VideoPlayer video_player_;
    entergram::SpriteRenderer renderer_;
    entergram::LayerManager layer_manager_;
    entergram::InputManager input_manager_;
    entergram::AudioPlayer audio_player_;
    std::vector<uint8_t> current_script_;
    int wait_frames_remaining_ = 0;
    std::string intro_video_path_;
    entergram::Texture video_texture_;

#if WITH_SDL2
    SDL_Window* window_ = nullptr;
    SDL_GLContext gl_context_ = nullptr;
    int window_w_ = VIRTUAL_WIDTH;
    int window_h_ = VIRTUAL_HEIGHT;
#endif
};

// =============================================================================
// Implementation
// =============================================================================

GameEngine::GameEngine() : state_(GameState::Boot) {
    callbacks_.engine = this;
}

// Callback implementations
void GameCallbacks::on_voice_play(const std::string& file_name, int volume, int) {
    if (engine) engine->play_voice(file_name, volume);
}
void GameCallbacks::on_voice_stop(int voice_id) { if (engine) engine->stop_voice(voice_id); }
void GameCallbacks::on_voice_wait(int voice_id) { if (engine) engine->wait_for_voice(voice_id); }
void GameCallbacks::on_bgm_play(const std::string& file_name, int volume) { if (engine) engine->play_bgm(file_name, volume); }
void GameCallbacks::on_bgm_stop() { if (engine) engine->stop_bgm(); }
void GameCallbacks::on_se_play(const std::string& file_name, int volume) { if (engine) engine->play_se(file_name, volume); }
void GameCallbacks::on_movie_play(const std::string& file_name) { if (engine) engine->play_movie(file_name); }
void GameCallbacks::on_movie_stop() {}
void GameCallbacks::on_text_display(const std::string& text, const std::string& name) { if (engine) engine->display_text(text, name); }
void GameCallbacks::on_choice(const std::vector<std::string>& options) { printf("[CHOICE] %zu options\n", options.size()); }
void GameCallbacks::on_system_call(uint32_t code, const std::string& data) { printf("[SYS] code=0x%x\n", code); }
void GameCallbacks::on_wait(int frames) { if (engine) engine->wait_frames(frames); }

bool GameEngine::load_script(const std::string& script_path) {
    auto file = rom_.extract_file(script_path);
    if (!file) {
        std::cerr << "ERROR: Cannot extract " << script_path << " from ROM\n";
        return false;
    }

    if (entergram::Snr0Parser::is_valid_snr0(file->data)) {
        current_script_ = entergram::Snr0Parser::parse(file->data);
        if (current_script_.empty()) {
            std::cerr << "ERROR: Failed to decompress " << script_path << "\n";
            return false;
        }
    } else {
        // Raw SNR: skip the 16-byte header, use bytecode directly
        size_t skip = 0;
        if (file->data.size() >= 4) {
            std::string magic(reinterpret_cast<const char*>(file->data.data()), 4);
            if (magic == "SNR " || magic == "SNR0") {
                skip = 16;  // Skip SNR header
            }
        }
        current_script_ = std::vector<uint8_t>(file->data.begin() + skip, file->data.end());
    }

    vm_.load_script(current_script_);
    return true;
}

bool GameEngine::extract_intro_video() {
    // Try extracting the intro video from movie/ directory.
    // Different Entergram games use different intro filenames:
    //   Umineko: movie/sakucs_op.mp4
    //   Higurashi: movie/mv01.mp4
    std::vector<std::string> intro_videos = {
        "movie/sakucs_op.mp4",   // Umineko
        "movie/mv01.mp4",        // Higurashi
        "movie/op1.mp4",
    };

    for (const auto& path : intro_videos) {
        auto movie = rom_.extract_file(path);
        if (movie && !movie->data.empty()) {
            intro_video_path_ = "intro_movie.mp4";

            std::ofstream f(intro_video_path_, std::ios::binary);
            f.write(reinterpret_cast<const char*>(movie->data.data()), movie->data.size());
            f.close();

            std::cout << "Extracted intro video: " << path << " ("
                      << movie->data.size() / (1024*1024) << " MB)\n";

            if (video_player_.open(intro_video_path_)) {
                std::cout << "  Video: " << video_player_.width() << "x"
                          << video_player_.height() << " @ "
                          << video_player_.frame_rate() << " fps\n";

                // Extract audio from the video
                if (video_player_.has_audio_stream()) {
                    printf("  Extracting audio...\n");
                    auto audio = video_player_.extract_audio();
                    if (!audio.empty()) {
                        audio_player_.play_pcm(audio, 48000, 1);
                        printf("  Audio: %zu samples playing\n", audio.size());
                    }
                }
                return true;
            }
        }
    }
    return false;
}

void GameEngine::render_video_frame() {
#if WITH_SDL2
    // Get current RGBA frame
    if (video_player_.has_pending_frame()) {
        auto frame = video_player_.read_frame();
        if (frame && frame->valid()) {
            // Upload to texture and render as fullscreen quad
            video_texture_.upload(frame->width, frame->height, frame->data.data());
            printf("[RENDER] Video texture uploaded: %dx%d, texture_id=%u\n",
                   frame->width, frame->height, video_texture_.gl_texture_id());
            entergram::Sprite sprite;
            sprite.x = 0;
            sprite.y = 0;
            sprite.width = static_cast<float>(frame->width);
            sprite.height = static_cast<float>(frame->height);
            renderer_.clear(0.0f, 0.0f, 0.0f, 1.0f);
            renderer_.render_video(sprite, video_texture_);
            video_player_.mark_frame_consumed();
        } else {
            renderer_.clear(0.0f, 0.0f, 0.0f, 1.0f);
            SDL_GL_SwapWindow(window_);
        }
    } else {
        // No pending frame: just show black (frame being decoded)
        renderer_.clear(0.0f, 0.0f, 0.0f, 1.0f);
        SDL_GL_SwapWindow(window_);
    }

    SDL_GL_SwapWindow(window_);
#endif
}

void GameEngine::update_video() {
    if (state_ != GameState::IntroVideo) return;

    // If video player has no stream (failed to open), skip to main menu
    if (video_player_.width() == 0 && video_player_.height() == 0) {
        state_ = GameState::MainMenu;
        printf("No intro video available, going to main menu\n");
        fflush(stdout);
        return;
    }

    // After audio extraction, seek video to beginning (EOF may have been reached)
    if (video_player_.is_eof()) {
        video_player_.seek(0.0);
        printf("[VIDEO] Seeked to start\n");
        fflush(stdout);
    }

    // Real-time frame scheduling using the video's frame rate
    double fps = video_player_.frame_rate();
    if (fps <= 0) fps = 29.97;
    double frame_duration_ms = 1000.0 / fps;

    static auto video_start = std::chrono::steady_clock::now();
    static int last_frame_index = -1;

    auto now = std::chrono::steady_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(now - video_start).count();
    int target_frame = static_cast<int>(elapsed_ms / frame_duration_ms);

    if (video_player_.is_eof() && !video_player_.has_pending_frame()) {
        state_ = GameState::MainMenu;
        printf("Intro video finished, showing SELECT menu\n");
        fflush(stdout);
        std::remove(intro_video_path_.c_str());
        return;
    }

    // Only decode/consume a new frame when we've reached the right frame index
    if (target_frame > last_frame_index) {
        last_frame_index = target_frame;
        printf("[VIDEO] Frame %d, has_frame=%d\n", target_frame, video_player_.has_pending_frame());
        fflush(stdout);

        if (video_player_.has_pending_frame()) {
            // Consume the buffered frame
            render_video_frame();
        } else {
            // Decode next frame
            video_player_.read_frame();
            if (video_player_.has_pending_frame()) {
                render_video_frame();
            }
        }
    }
}

bool GameEngine::initialize(const std::string& rom_path, bool list_mode) {
    std::cout << "=== Entergram Emulator v0.1.0 ===\n";
    std::cout << "Loading ROM: " << rom_path << "\n";
    std::cout.flush();

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

    const auto& root = rom_.root();
    std::cout << "Root directory:\n";
    for (const auto& entry : root.children) {
        std::cout << "  " << entry.name << "/ (" << (entry.is_directory ? "dir" : "file") << ")\n";
    }

    // If --list flag, dump movie/ directory contents and exit
    if (list_mode) {
        std::cout << "\nListing movie/ directory:\n";
        auto movie_dir = rom_.find_directory("movie");
        if (movie_dir) {
            for (const auto& entry : (*movie_dir)->children) {
                std::cout << "  " << entry.name;
                if (entry.is_directory) {
                    std::cout << "/ (dir)\n";
                } else {
                    std::cout << " (" << (entry.data_size / (1024*1024)) << "MB)\n";
                }
            }
        } else {
            std::cout << "  (movie/ directory not found)\n";
        }
        return true;
    }

    if (!load_script("main.snr")) {
        std::cerr << "WARNING: Cannot load main.snr\n";
    }

#if WITH_SDL2
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    // Get display bounds for proper window sizing
    SDL_DisplayMode dm;
    SDL_GetDesktopDisplayMode(0, &dm);
    window_w_ = dm.w;
    window_h_ = dm.h;

    window_ = SDL_CreateWindow("Entergram Emulator - Umineko",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        window_w_, window_h_, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    if (!window_) {
        std::cerr << "Failed to create window: " << SDL_GetError() << "\n";
        SDL_Quit();
        return false;
    }

    gl_context_ = SDL_GL_CreateContext(window_);
    if (!gl_context_) {
        std::cerr << "Failed to create OpenGL context: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window_);
        SDL_Quit();
        return false;
    }

    // VSync for smooth video playback
    SDL_GL_SetSwapInterval(1);

    if (!renderer_.initialize()) {
        std::cerr << "Failed to initialize renderer\n";
        return false;
    }
    renderer_.set_viewport(window_w_, window_h_);
    audio_player_.initialize();

    // Extract intro video with graceful error handling
    std::cout << "Extracting intro video...\n";
    if (!extract_intro_video()) {
        std::cerr << "WARNING: Cannot extract intro video - skipping intro\n";
        std::cerr.flush();
    }
#endif

    state_ = GameState::IntroVideo;
    return true;
}

void GameEngine::handle_input() {
#if WITH_SDL2
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_QUIT: state_ = GameState::Shutdown; break;
            case SDL_KEYDOWN:
                if (e.key.keysym.sym == SDLK_ESCAPE) state_ = GameState::Shutdown;
                if (e.key.keysym.sym == SDLK_SPACE) {
                    if (state_ == GameState::IntroVideo) {
                        state_ = GameState::MainMenu;
                    }
                }
                break;
        }
    }
#endif
}

void GameEngine::render() {
#if WITH_SDL2
    // For intro video, frames are rendered in update_video()
    if (state_ != GameState::IntroVideo) {
        renderer_.clear(0.0f, 0.0f, 0.0f, 1.0f);
        auto render_list = layer_manager_.get_render_list();
        for (const auto* props : render_list) {
            (void)props;
        }
        SDL_GL_SwapWindow(window_);
    }
#endif
}

// Game callbacks
void GameEngine::play_voice(const std::string& file_name, int volume) {
    printf("VOICE: %s (vol=%d)\n", file_name.c_str(), volume);
    // Extract and decode NXA voice file
    auto file = rom_.extract_file("voice/" + file_name);
    if (!file) {
        printf("  Voice file not found: %s\n", file_name.c_str());
        return;
    }

    entergram::NxaDecoder decoder;
    std::vector<int16_t> pcm = decoder.decode(file->data);
    if (pcm.empty()) {
        printf("  Decode error: %s\n", decoder.last_error().c_str());
        return;
    }

    printf("  Decoded: %zu samples @ %u Hz\n",
           pcm.size(), 48000);

    audio_player_.set_volume(volume / 100.0f);
    audio_player_.play_pcm(pcm, 48000, 1);
}
void GameEngine::stop_voice(int) {}
void GameEngine::wait_for_voice(int) {}
void GameEngine::play_bgm(const std::string& file_name, int volume) {
    printf("BGM: %s (vol=%d)\n", file_name.c_str(), volume);
    // BGM files are in bgm/ directory with .nxa extension
    auto file = rom_.extract_file("bgm/" + file_name);
    if (!file) {
        // Try without extension
        auto dot_pos = file_name.find('.');
        std::string base = dot_pos != std::string::npos
            ? file_name.substr(0, dot_pos) : file_name;
        auto file2 = rom_.extract_file("bgm/" + base + ".nxa");
        if (!file2) {
            printf("  BGM file not found\n");
            return;
        }
        file = file2;
    }

    auto info = entergram::parse_nxa(file->data);
    if (!info) {
        printf("  Not an NXA file\n");
        return;
    }

    entergram::NxaDecoder decoder;
    std::vector<int16_t> pcm = decoder.decode(file->data);
    if (pcm.empty()) {
        printf("  Decode error: %s\n", decoder.last_error().c_str());
        return;
    }

    printf("  BGM decoded: %zu samples @ %u Hz\n", pcm.size(), info->sample_rate);

    audio_player_.set_volume(volume / 100.0f);
    audio_player_.play_pcm(pcm, info->sample_rate, info->channel_count);
}
void GameEngine::stop_bgm() {}
void GameEngine::play_se(const std::string& file_name, int volume) {
    printf("SE: %s (vol=%d)\n", file_name.c_str(), volume);
}

void GameEngine::play_movie(const std::string& file_name) {
    printf("MOVIE: %s\n", file_name.c_str());
    auto movie = rom_.extract_file("movie/" + file_name);
    if (movie && !movie->data.empty()) {
        std::string temp_path = "movie_temp.mp4";
        std::ofstream f(temp_path, std::ios::binary);
        f.write(reinterpret_cast<const char*>(movie->data.data()), movie->data.size());
        f.close();

        if (video_player_.open(temp_path)) {
            printf("  Opened: %dx%d @ %.1ffps, %.1f MB\n",
                   video_player_.width(), video_player_.height(),
                   video_player_.frame_rate(),
                   movie->data.size() / (1024.0 * 1024.0));
            state_ = GameState::IntroVideo;
        } else {
            printf("  ERROR: %s\n", video_player_.last_error().c_str());
        }
    } else {
        printf("  Movie not found in ROM\n");
    }
}

void GameEngine::display_text(const std::string& text, const std::string& character_name) {
    printf("[TEXT] %s: %s\n", character_name.c_str(), text.c_str());
}
void GameEngine::wait_frames(int frames) { wait_frames_remaining_ = frames; }

#if WITH_SDL2
int GameEngine::run_sdl() {
    using clock = std::chrono::steady_clock;
    const double target_frame_time = 1000.0 / 60.0;

    while (state_ != GameState::Shutdown) {
        auto frame_start = clock::now();

        handle_input();

        switch (state_) {
            case GameState::IntroVideo:
                // Run VM in background (processes MOVIEPLAY/SYS callbacks)
                if (vm_.is_running() && !vm_.is_waiting_for_voice()) {
                    vm_.step(&callbacks_);
                }
                update_video();
                break;
            case GameState::MainMenu:
                if (vm_.is_running()) vm_.run(&callbacks_);
                render();
                break;
            case GameState::Novel:
                if (wait_frames_remaining_ > 0) {
                    wait_frames_remaining_--;
                } else if (vm_.is_running()) {
                    vm_.step(&callbacks_);
                }
                render();
                break;
            case GameState::Boot:
                state_ = GameState::IntroVideo;
                break;
            default: break;
        }

        auto elapsed = std::chrono::duration<double, std::milli>(clock::now() - frame_start).count();
        if (elapsed < target_frame_time) {
            std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(target_frame_time - elapsed)));
        }
    }

    // Cleanup
    if (!intro_video_path_.empty()) {
        std::remove(intro_video_path_.c_str());
    }
#if WITH_SDL2
    if (gl_context_) SDL_GL_DeleteContext(gl_context_);
    if (window_) SDL_DestroyWindow(window_);
    SDL_Quit();
#endif
    return 0;
}
#endif

// =============================================================================
// ROM Discovery & Interactive Menu
// =============================================================================

struct RomEntry {
    std::string path;
    std::string name;
    uint64_t size = 0;
};

static std::vector<RomEntry> scan_all_roms() {
    std::vector<RomEntry> roms;
    namespace fs = std::filesystem;

    std::vector<fs::path> search_dirs;

#ifdef _WIN32
    // Executable directory
    char exe_path[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, exe_path, MAX_PATH);
    if (len > 0) {
        search_dirs.push_back(fs::path(exe_path).parent_path());
    }
#endif
    // Current working directory
    search_dirs.push_back(fs::current_path());

    for (const auto& dir : search_dirs) {
        // data.rom in this dir
        fs::path candidate = dir / "data.rom";
        if (fs::exists(candidate)) {
            auto sz = fs::file_size(candidate);
            roms.push_back({candidate.string(), "data.rom", sz});
        }
        // games/<juego>/data.rom
        fs::path games_dir = dir / "games";
        if (fs::exists(games_dir) && fs::is_directory(games_dir)) {
            for (const auto& game_entry : fs::directory_iterator(games_dir)) {
                if (game_entry.is_directory()) {
                    fs::path rom = game_entry.path() / "data.rom";
                    if (fs::exists(rom)) {
                        auto sz = fs::file_size(rom);
                        roms.push_back({rom.string(), game_entry.path().filename().string() + "/data.rom", sz});
                    }
                }
            }
        }
        // roms.txt
        fs::path roms_txt = dir / "roms.txt";
        if (fs::exists(roms_txt)) {
            std::ifstream f(roms_txt);
            std::string line;
            while (std::getline(f, line)) {
                line.erase(0, line.find_first_not_of(" \t\r\n"));
                line.erase(line.find_last_not_of(" \t\r\n") + 1);
                if (!line.empty() && fs::exists(line)) {
                    auto sz = fs::file_size(line);
                    fs::path p(line);
                    // Use filename (data.rom) if path ends with it, otherwise use parent dir
                    std::string game_name;
                    if (p.filename().string() == "data.rom") {
                        game_name = p.parent_path().filename().string();
                        if (game_name == "romfs" || game_name.empty()) {
                            // Try grandparent (e.g. for eden/dump/01006A300BA2C000/romfs/data.rom)
                            game_name = p.parent_path().parent_path().filename().string();
                        }
                    }
                    if (game_name.empty()) game_name = p.filename().string();
                    roms.push_back({line, game_name, sz});
                }
            }
        }
    }

    // Deduplicate by path
    std::sort(roms.begin(), roms.end(), [](const RomEntry& a, const RomEntry& b) {
        return a.path < b.path;
    });
    roms.erase(std::unique(roms.begin(), roms.end(), [](const RomEntry& a, const RomEntry& b) {
        return a.path == b.path;
    }), roms.end());

    return roms;
}

// Simple interactive ROM selector using SDL2 + OpenGL (no external GUI lib needed)
static std::optional<std::string> select_rom_interactive(
    const std::vector<RomEntry>& roms,
    SDL_Window* window, SDL_GLContext ctx) {

    // Use the already-initialized renderer
    entergram::SpriteRenderer renderer;
    // We need a simple way to render text. Use printf to stderr for now
    // and handle keyboard input directly.

    std::cerr << "\n=== ROM Selection Menu ===\n";
    std::cerr << "Multiple data.rom files found. Select which to load:\n\n";
    for (size_t i = 0; i < roms.size(); i++) {
        char buf[256];
        snprintf(buf, sizeof(buf), "  [%zu] %s (%.1f GB) - %s\n",
                 i + 1, roms[i].name.c_str(),
                 (double)roms[i].size / (1024.0 * 1024.0 * 1024.0),
                 roms[i].path.c_str());
        std::cerr << buf;
    }
    std::cerr << "\n  [0] Cancel / Quit\n";
    std::cerr << "  Use mouse or type number (1-" << roms.size() << "): ";
    std::cerr.flush();

    SDL_Event e;
    while (true) {
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_QUIT:
                    return std::nullopt;
                case SDL_KEYDOWN:
                    if (e.key.keysym.sym == SDLK_ESCAPE) {
                        return std::nullopt;
                    }
                    if (e.key.keysym.sym >= SDLK_0 && e.key.keysym.sym <= SDLK_9) {
                        int choice = e.key.keysym.sym - SDLK_0;
                        if (choice == 0) return std::nullopt;
                        if (choice >= 1 && (size_t)choice <= roms.size()) {
                            std::cerr << "\nSelected: " << roms[choice - 1].path << "\n";
                            return roms[choice - 1].path;
                        }
                    }
                    if (e.key.keysym.sym >= SDLK_PLUS && e.key.keysym.sym <= SDLK_9) {
                        // Numpad numbers
                        int choice = e.key.keysym.sym - SDLK_PLUS + 1;
                        if (choice == 10) choice = 0;
                        if (choice == 0) return std::nullopt;
                        if (choice >= 1 && (size_t)choice <= roms.size()) {
                            std::cerr << "\nSelected: " << roms[choice - 1].path << "\n";
                            return roms[choice - 1].path;
                        }
                    }
                    if (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_KP_ENTER) {
                        // Default to first
                        std::cerr << "\nSelected: " << roms[0].path << "\n";
                        return roms[0].path;
                    }
                    break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}


// =============================================================================
// main
// =============================================================================
int main(int argc, char* argv[]) {
    bool list_mode = false;
    std::string rom_path;

    // Parse args
    for (int i = 1; i < argc; i++) {
        std::string arg(argv[i]);
        if (arg == "--list" || arg == "-l") {
            list_mode = true;
        } else {
            rom_path = arg;
        }
    }

    if (rom_path.empty() && !list_mode) {
        // Scan for all ROMs in known locations
        auto roms = scan_all_roms();
        
        if (roms.empty()) {
            std::cerr << "No data.rom found.\n";
            std::cerr << "Usage: " << argv[0] << " <path_to_data.rom>\n";
            std::cerr << "Or place data.rom next to the emulator, in games/<juego>/, or in roms.txt\n";
            return 1;
        }

        if (roms.size() == 1) {
            // Single ROM found - use it directly
            rom_path = roms[0].path;
            std::cerr << "Found: " << rom_path << "\n";
        } else {
            // Multiple ROMs found - show interactive menu
            std::cerr << "Found " << roms.size() << " ROMs:\n";
            for (size_t i = 0; i < roms.size(); i++) {
                std::cerr << "  [" << (i + 1) << "] " << roms[i].name 
                          << " (" << (double)roms[i].size / (1024.0*1024.0*1024.0) << " GB)\n";
            }
            std::cerr << "Select ROM (1-" << roms.size() << "), 0 to cancel: ";
            std::cerr.flush();

            // Read from stdin (terminal or piped)
            std::cerr.flush();
            std::string input;
            if (std::getline(std::cin, input)) {
                try {
                    int choice = std::stoi(input);
                    if (choice == 0) {
                        std::cerr << "Cancelled.\n";
                        return 1;
                    }
                    if (choice > 0 && choice <= (int)roms.size()) {
                        rom_path = roms[choice - 1].path;
                        std::cerr << "Selected: " << rom_path << "\n";
                    } else {
                        rom_path = roms[0].path;
                        std::cerr << "Invalid choice, using: " << rom_path << "\n";
                    }
                } catch (...) {
                    rom_path = roms[0].path;
                    std::cerr << "Using: " << rom_path << "\n";
                }
            } else {
                rom_path = roms[0].path;
                std::cerr << "Using: " << rom_path << "\n";
            }
        }
    }

#if WITH_SDL2
    GameEngine engine;
    if (!engine.initialize(rom_path, list_mode)) {
        return 1;
    }
    return engine.run_sdl();
#else
    GameCallbacks callbacks;
    GameEngine engine;
    if (!engine.initialize(rom_path, list_mode)) {
        return 1;
    }
    callbacks.engine = &engine;
    engine.vm().run(&callbacks);
    return 0;
#endif
}
