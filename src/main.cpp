#include "palette.hpp"
#include "screen.hpp"
#include "wad.hpp"

#include <SDL.h>

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

constexpr int kWindowWidth = 960;
constexpr int kWindowHeight = 600;
constexpr int kTargetFps = 60;

std::atomic<bool> g_quit{false};

void on_signal(int /*signum*/) {
    g_quit = true;
}

void install_signal_handlers() {
    struct sigaction action {};
    action.sa_handler = on_signal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGINT, &action, nullptr);
    sigaction(SIGTERM, &action, nullptr);
}

bool has_flag(int argc, char* argv[], const char* flag) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], flag) == 0) {
            return true;
        }
    }
    return false;
}

const char* wad_path_arg(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] != '-') {
            return argv[i];
        }
    }
    return "wads/DOOM.WAD";
}

void fatal(const char* message) {
    SDL_Log("%s: %s", message, SDL_GetError());
    SDL_Quit();
    std::exit(EXIT_FAILURE);
}

}  // namespace

int main(int argc, char* argv[]) {
    install_signal_handlers();

    const std::string wad_path = wad_path_arg(argc, argv);
    const bool wad_only = has_flag(argc, argv, "--wad-only");

    const auto wad = Wad::load(wad_path);
    if (!wad) {
        std::fprintf(stderr, "Failed to load WAD: %s\n", wad_path.c_str());
        return EXIT_FAILURE;
    }
    wad->print_directory();

    if (wad_only) {
        return EXIT_SUCCESS;
    }

    const auto palette = Palette::load_from_wad(*wad);
    if (!palette) {
        return EXIT_FAILURE;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fatal("SDL_Init failed");
    }

    SDL_Window* window =
        SDL_CreateWindow("doom", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, kWindowWidth,
                         kWindowHeight, SDL_WINDOW_SHOWN);
    if (!window) {
        fatal("SDL_CreateWindow failed");
    }

    SDL_Renderer* renderer =
        SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        fatal("SDL_CreateRenderer failed");
    }

    Screen screen;
    if (!screen.init(renderer)) {
        fatal("Screen::init failed");
    }

    screen.draw_palette_test(*palette);

    bool running = true;
    const Uint32 frame_ms = 1000u / static_cast<Uint32>(kTargetFps);
    while (running && !g_quit) {
        const Uint32 frame_start = SDL_GetTicks();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            }
        }

        screen.present(renderer, *palette);

        const Uint32 elapsed = SDL_GetTicks() - frame_start;
        if (elapsed < frame_ms) {
            SDL_Delay(frame_ms - elapsed);
        }
    }

    screen.shutdown();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_SUCCESS;
}
