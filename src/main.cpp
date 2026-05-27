#include <SDL.h>

#include <cstdlib>

namespace {

constexpr int kWindowWidth = 800;
constexpr int kWindowHeight = 600;

void fatal(const char* message) {
    SDL_Log("%s: %s", message, SDL_GetError());
    SDL_Quit();
    std::exit(EXIT_FAILURE);
}

}  // namespace

int main(int /*argc*/, char* /*argv*/[]) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fatal("SDL_Init failed");
    }

    SDL_Window* window =
        SDL_CreateWindow("doom", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         kWindowWidth, kWindowHeight, SDL_WINDOW_SHOWN);
    if (!window) {
        fatal("SDL_CreateWindow failed");
    }

    SDL_Renderer* renderer =
        SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        fatal("SDL_CreateRenderer failed");
    }

    const SDL_Vertex vertices[] = {
        {{static_cast<float>(kWindowWidth) * 0.5f, 80.0f}, {255, 80, 80, 255}, {0, 0}},
        {{120.0f, static_cast<float>(kWindowHeight) - 80.0f}, {80, 255, 120, 255}, {0, 0}},
        {{static_cast<float>(kWindowWidth) - 120.0f, static_cast<float>(kWindowHeight) - 80.0f},
         {80, 140, 255, 255},
         {0, 0}},
    };
    const int indices[] = {0, 1, 2};

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            }
        }

        SDL_SetRenderDrawColor(renderer, 24, 24, 32, 255);
        SDL_RenderClear(renderer);

        if (SDL_RenderGeometry(renderer, nullptr, vertices, 3, indices, 3) != 0) {
            fatal("SDL_RenderGeometry failed");
        }

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_SUCCESS;
}
