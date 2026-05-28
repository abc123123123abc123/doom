#pragma once

#include "map.hpp"
#include "palette.hpp"
#include "screen.hpp"
#include "wad.hpp"

class Render3D {
public:
    void render(Screen& screen, const Wad& wad, const Map& map,
                const std::vector<MapLineState>& lines,
                const std::vector<MapThingState>& things, const Palette& palette,
                float player_x, float player_y, float player_angle, int wall_cycle,
                int ceiling_cycle, int floor_cycle, int light_cycle, int gametic) const;

private:
    struct Hit {
        float distance = 0.0f;
        int line_index = -1;
        float wall_offset = 0.0f;
        float wall_u = 0.0f;
        bool is_boundary = false;
    };

    bool trace_ray(const Map& map, const std::vector<MapLineState>& lines, float origin_x,
                   float origin_y, float dir_x, float dir_y, Hit& hit) const;

    int shade_for_distance(float distance) const;
    const char* texture_name_for_line(const MapLine& line) const;
};
