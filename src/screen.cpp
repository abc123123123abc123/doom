#include "screen.hpp"

#include <algorithm>
#include <cstdio>

namespace {

SDL_Rect centered_rect(int window_w, int window_h, int src_w, int src_h) {
    const int scale = std::max(1, std::min(window_w / src_w, window_h / src_h));
    const int dst_w = src_w * scale;
    const int dst_h = src_h * scale;
    SDL_Rect rect;
    rect.x = (window_w - dst_w) / 2;
    rect.y = (window_h - dst_h) / 2;
    rect.w = dst_w;
    rect.h = dst_h;
    return rect;
}

}  // namespace

bool Screen::init(SDL_Renderer* renderer) {
    renderer_ = renderer;
    texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                 kWidth, kHeight);
    if (!texture_) {
        std::fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_SetTextureBlendMode(texture_, SDL_BLENDMODE_NONE);
    SDL_SetTextureScaleMode(texture_, SDL_ScaleModeNearest);
    rgba_.resize(static_cast<std::size_t>(kWidth * kHeight));
    clear(0);
    return true;
}

void Screen::shutdown() {
    if (texture_) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }
    renderer_ = nullptr;
}

void Screen::clear(std::uint8_t color) {
    indices_.fill(color);
}

void Screen::fill_rect(int x, int y, int w, int h, std::uint8_t color) {
    const int x0 = std::max(0, x);
    const int y0 = std::max(0, y);
    const int x1 = std::min(kWidth, x + w);
    const int y1 = std::min(kHeight, y + h);
    for (int row = y0; row < y1; ++row) {
        std::fill(indices_.begin() + row * kWidth + x0, indices_.begin() + row * kWidth + x1,
                  color);
    }
}

void Screen::draw_palette_test(const Palette& palette) {
    clear(palette.map_index(0));

    for (int band = 0; band < 16; ++band) {
        const int y = band * (kHeight / 16);
        const int h = kHeight / 16;
        const std::uint8_t color = static_cast<std::uint8_t>(band * 16);
        fill_rect(0, y, kWidth / 2, h, palette.map_index(color));
    }

    for (int x = 0; x < kWidth / 2; ++x) {
        const std::uint8_t color = static_cast<std::uint8_t>((x * 255) / (kWidth / 2));
        for (int y = 0; y < kHeight; ++y) {
            indices_[static_cast<std::size_t>(y * kWidth + (kWidth / 2 + x))] =
                palette.map_index(color);
        }
    }
}

void Screen::present(SDL_Renderer* renderer, const Palette& palette) {
    for (int i = 0; i < kWidth * kHeight; ++i) {
        const std::uint8_t mapped = palette.map_index(indices_[static_cast<std::size_t>(i)]);
        std::uint8_t r = 0;
        std::uint8_t g = 0;
        std::uint8_t b = 0;
        palette.color_rgb(mapped, r, g, b);
        rgba_[static_cast<std::size_t>(i)] =
            (0xFFu << 24) | (static_cast<std::uint32_t>(r) << 16) |
            (static_cast<std::uint32_t>(g) << 8) | static_cast<std::uint32_t>(b);
    }

    if (SDL_UpdateTexture(texture_, nullptr, rgba_.data(), kWidth * static_cast<int>(sizeof(std::uint32_t))) != 0) {
        std::fprintf(stderr, "SDL_UpdateTexture failed: %s\n", SDL_GetError());
        return;
    }

    int window_w = 0;
    int window_h = 0;
    SDL_GetRendererOutputSize(renderer, &window_w, &window_h);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    const SDL_Rect dst = centered_rect(window_w, window_h, kWidth, kHeight);
    SDL_RenderCopy(renderer, texture_, nullptr, &dst);
    SDL_RenderPresent(renderer);
}
