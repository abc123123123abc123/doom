#pragma once

#include "screen.hpp"
#include "wad.hpp"

#include <cstdint>
#include <vector>

struct MapPoint {
    std::int16_t x = 0;
    std::int16_t y = 0;
};

struct MapLine {
    std::int16_t v1 = 0;
    std::int16_t v2 = 0;
};

struct MapThing {
    std::int16_t x = 0;
    std::int16_t y = 0;
    std::int16_t type = 0;
};

class Map {
public:
    static bool load_from_wad(const Wad& wad, Map& map);

    void draw(Screen& screen, std::uint8_t line_color, std::uint8_t point_color,
              std::uint8_t thing_color) const;

    int point_count() const { return static_cast<int>(points_.size()); }
    int line_count() const { return static_cast<int>(lines_.size()); }

private:
    std::vector<MapPoint> points_;
    std::vector<MapLine> lines_;
    std::vector<MapThing> things_;
};
