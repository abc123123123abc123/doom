#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

class Wad;

class Palette {
public:
    static std::optional<Palette> load_from_wad(const Wad& wad);

    void color_rgb(std::uint8_t index, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b) const;
    std::uint8_t map_index(std::uint8_t index, int light = 0) const;

private:
    std::array<std::uint8_t, 768> playpal_{};
    std::vector<std::uint8_t> colormap_;
};
