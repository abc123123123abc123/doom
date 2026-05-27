#pragma once

#include "map.hpp"
#include "screen.hpp"
#include "wad.hpp"

class Render3D {
public:
    void render(Screen& screen, const Wad& wad, const Map& map,
                const std::vector<MapLineState>& lines, float player_x, float player_y,
                float player_angle) const;

private:
    struct Hit {
        float distance = 0.0f;
        int line_index = -1;
        float wall_offset = 0.0f;
    };

    bool trace_ray(const Map& map, const std::vector<MapLineState>& lines, float origin_x,
                   float origin_y, float dir_x, float dir_y, Hit& hit) const;

    std::uint8_t shade_for_distance(float distance) const;
    const char* texture_name_for_line(int line_index) const;
};
