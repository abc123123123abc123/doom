#include "map.hpp"
#include "palette.hpp"
#include "patch.hpp"
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

enum class View {
    Title,
    Map,
    Sprite,
};

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

void redraw_view(View view, Screen& screen, const Wad& wad, const Map& map) {
    screen.clear(0);

    switch (view) {
        case View::Title: {
            const auto lump = wad.find_lump("TITLEPIC");
            if (!lump || !draw_patch(screen, wad.lump_data(*lump), 0, 0)) {
                std::fprintf(stderr, "Failed to draw TITLEPIC\n");
            }
            break;
        }
        case View::Map:
            map.draw(screen, 176, 112, 215);
            break;
        case View::Sprite: {
            const auto lump = wad.find_lump("TROOA1");
            if (!lump || !draw_patch(screen, wad.lump_data(*lump), Screen::kWidth / 2,
                                     Screen::kHeight / 2)) {
                std::fprintf(stderr, "Failed to draw TROOA1\n");
            }
            break;
        }
    }
}

const Palette& palette_for_view(View view, const Palette& title_palette,
                                const Palette& game_palette) {
    return view == View::Title ? title_palette : game_palette;
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

    const auto game_palette = Palette::load_from_wad(*wad);
    if (!game_palette) {
        return EXIT_FAILURE;
    }

    auto title_palette = Palette::load_lump(*wad, "TITLEPAL");
    if (!title_palette) {
        title_palette = game_palette;
    }

    Map map;
    if (!Map::load_from_wad(*wad, map)) {
        return EXIT_FAILURE;
    }

    if (!wad->find_lump("TITLEPIC") || !wad->find_lump("TROOA1")) {
        std::fprintf(stderr, "TITLEPIC or TROOA1 lump not found\n");
        return EXIT_FAILURE;
    }

    std::printf("Keys: 1=title  2=map  3=sprite  Esc=quit\n");

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

    View view = View::Title;
    redraw_view(view, screen, *wad, map);

    bool running = true;
    const Uint32 frame_ms = 1000u / static_cast<Uint32>(kTargetFps);
    while (running && !g_quit) {
        const Uint32 frame_start = SDL_GetTicks();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                }
                if (event.key.keysym.sym == SDLK_1) {
                    view = View::Title;
                    redraw_view(view, screen, *wad, map);
                }
                if (event.key.keysym.sym == SDLK_2) {
                    view = View::Map;
                    redraw_view(view, screen, *wad, map);
                }
                if (event.key.keysym.sym == SDLK_3) {
                    view = View::Sprite;
                    redraw_view(view, screen, *wad, map);
                }
            }
        }

        screen.present(renderer, palette_for_view(view, *title_palette, *game_palette));

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
