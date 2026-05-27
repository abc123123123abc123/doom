#pragma once

#include "palette.hpp"

#include <SDL.h>

#include <array>
#include <cstdint>
#include <vector>

class Screen {
public:
    static constexpr int kWidth = 320;
    static constexpr int kHeight = 200;

    bool init(SDL_Renderer* renderer);
    void shutdown();

    void clear(std::uint8_t color);
    void put_pixel(int x, int y, std::uint8_t color);
    void fill_rect(int x, int y, int w, int h, std::uint8_t color);
    void draw_palette_test(const Palette& palette);

    void present(SDL_Renderer* renderer, const Palette& palette);

private:
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    std::array<std::uint8_t, kWidth * kHeight> indices_{};
    std::vector<std::uint32_t> rgba_;
};
