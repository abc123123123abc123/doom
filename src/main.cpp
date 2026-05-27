#include "game.hpp"
#include "map.hpp"
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

void update_window_title(SDL_Window* window, const Game& game) {
    char title[96];
    if (game.view == View::Map || game.view == View::World) {
        std::snprintf(title, sizeof(title), "doom - tic %d  pos (%d,%d)", game.gametic,
                      game.player_x(), game.player_y());
    } else {
        std::snprintf(title, sizeof(title), "doom - tic %d", game.gametic);
    }
    SDL_SetWindowTitle(window, title);
}

GameInput read_input(const Uint8* keys, View view, bool& use_was_down) {
    GameInput input;

    if (view == View::World) {
        input.move_north = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP];
        input.move_south = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN];
        input.move_west = keys[SDL_SCANCODE_A];
        input.move_east = keys[SDL_SCANCODE_D];
        input.turn_left = keys[SDL_SCANCODE_LEFT];
        input.turn_right = keys[SDL_SCANCODE_RIGHT];
    } else {
        input.move_north = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP];
        input.move_south = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN];
        input.move_east = keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT];
        input.move_west = keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT];
    }

    const bool use_down = keys[SDL_SCANCODE_E];
    input.use = use_down && !use_was_down;
    use_was_down = use_down;
    return input;
}

const Palette& palette_for_view(View view, const Palette& title_palette,
                                const Palette& game_palette) {
    return view == View::Title ? title_palette : game_palette;
}

bool handle_keydown(SDL_Keycode key, Game& game) {
    switch (key) {
        case SDLK_ESCAPE:
            game.quit_requested = true;
            return true;
        case SDLK_1:
            game.set_view(View::Title);
            return true;
        case SDLK_2:
            game.set_view(View::Map);
            return true;
        case SDLK_3:
            game.set_view(View::Sprite);
            return true;
        case SDLK_4:
            game.set_view(View::World);
            return true;
        default:
            return false;
    }
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

    std::printf("Keys: 1=title  2=map  3=sprite  4=world3d  Esc=quit\n");
    std::printf("Map: WASD  |  World: WASD move, arrows turn, E=use  |  %d tics/sec\n",
                Game::kTicRate);

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

    Game game;
    game.init_from_map(map);
    game.draw(screen, *wad, map);
    update_window_title(window, game);

    double tic_accumulator = 0.0;
    Uint32 last_ticks = SDL_GetTicks();
    bool use_was_down = false;

    while (!game.quit_requested && !g_quit) {
        const Uint32 now = SDL_GetTicks();
        tic_accumulator += static_cast<double>(now - last_ticks) / 1000.0;
        last_ticks = now;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                game.quit_requested = true;
            }
            if (event.type == SDL_KEYDOWN) {
                handle_keydown(event.key.keysym.sym, game);
            }
        }

        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        const GameInput input = read_input(keys, game.view, use_was_down);

        while (tic_accumulator >= Game::kSecondsPerTic) {
            game.run_tic(input, map);
            tic_accumulator -= Game::kSecondsPerTic;
            update_window_title(window, game);
        }

        if (game.needs_redraw) {
            game.draw(screen, *wad, map);
            game.needs_redraw = false;
        }

        screen.present(renderer, palette_for_view(game.view, *title_palette, *game_palette));
    }

    screen.shutdown();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_SUCCESS;
}
