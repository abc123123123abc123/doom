#pragma once

#include "screen.hpp"

#include <cstdint>
#include <vector>

struct MapPoint {
    std::int16_t x = 0;
    std::int16_t y = 0;
};

struct MapLine {
    std::int16_t v1 = 0;
    std::int16_t v2 = 0;
    std::int16_t special = 0;

    bool is_door() const { return special == 376 || special == 448; }
};

struct MapLineState {
    MapLine line;
    bool door_open = false;

    bool blocks() const {
        if (line.is_door()) {
            return !door_open;
        }
        return true;
    }
};

struct MapThing {
    std::int16_t x = 0;
    std::int16_t y = 0;
    std::int16_t type = 0;
};

struct MapBounds {
    int min_x = 0;
    int max_x = 0;
    int min_y = 0;
    int max_y = 0;
};

struct MapThingState {
    MapThing thing;
    bool activated = false;
};

class Wad;

class Map {
public:
    static constexpr int kPlayerRadius = 10;

    static bool load_from_wad(const Wad& wad, Map& map);

    MapBounds bounds() const;
    bool is_position_valid(std::int16_t x, std::int16_t y,
                            const std::vector<MapLineState>& lines) const;
    void try_move(std::int16_t& x, std::int16_t& y, int dx, int dy,
                  const std::vector<MapLineState>& lines) const;

    void draw(Screen& screen, std::uint8_t line_color, std::uint8_t door_color,
              std::uint8_t point_color, std::uint8_t thing_color, std::uint8_t player_color,
              std::int16_t player_x, std::int16_t player_y,
              const std::vector<MapThingState>& things,
              const std::vector<MapLineState>& lines) const;

    int point_count() const { return static_cast<int>(points_.size()); }
    int line_count() const { return static_cast<int>(lines_.size()); }
    const std::vector<MapPoint>& points() const { return points_; }
    const std::vector<MapThing>& things() const { return things_; }
    const std::vector<MapLine>& lines() const { return lines_; }

private:
    std::vector<MapPoint> points_;
    std::vector<MapLine> lines_;
    std::vector<MapThing> things_;
};
