#include "palette.hpp"

#include "wad.hpp"

#include <cstdio>
#include <cstring>

namespace {

constexpr int kPaletteColors = 256;
constexpr int kPaletteBytes = kPaletteColors * 3;
constexpr int kColormapLightLevels = 32;

}  // namespace

std::optional<Palette> Palette::load_from_wad(const Wad& wad) {
    const auto pal_index = wad.find_lump("PLAYPAL");
    if (!pal_index) {
        std::fprintf(stderr, "PLAYPAL lump not found\n");
        return std::nullopt;
    }

    const WadLumpData pal_data = wad.lump_data(*pal_index);
    if (pal_data.size < static_cast<std::size_t>(kPaletteBytes)) {
        std::fprintf(stderr, "PLAYPAL too small (%zu bytes)\n", pal_data.size);
        return std::nullopt;
    }

    Palette palette;
    std::memcpy(palette.playpal_.data(), pal_data.data, kPaletteBytes);

    const auto map_index = wad.find_lump("COLORMAP");
    if (!map_index) {
        std::fprintf(stderr, "COLORMAP lump not found\n");
        return std::nullopt;
    }

    const WadLumpData map_data = wad.lump_data(*map_index);
    if (map_data.size < static_cast<std::size_t>(kPaletteColors * kColormapLightLevels)) {
        std::fprintf(stderr, "COLORMAP too small (%zu bytes)\n", map_data.size);
        return std::nullopt;
    }

    palette.colormap_.assign(map_data.data, map_data.data + map_data.size);
    return palette;
}

void Palette::color_rgb(std::uint8_t index, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b) const {
    const std::size_t offset = static_cast<std::size_t>(index) * 3u;
    r = playpal_[offset];
    g = playpal_[offset + 1];
    b = playpal_[offset + 2];
}

std::uint8_t Palette::map_index(std::uint8_t index, int light) const {
    if (colormap_.empty()) {
        return index;
    }
    if (light < 0) {
        light = 0;
    }
    if (light >= kColormapLightLevels) {
        light = kColormapLightLevels - 1;
    }
    const std::size_t offset =
        static_cast<std::size_t>(light) * kPaletteColors + static_cast<std::size_t>(index);
    return colormap_[offset];
}
