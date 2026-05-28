#include "map.hpp"

#include "wad.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace {

std::int32_t read_i32(const std::uint8_t* bytes) {
    std::int32_t value;
    std::memcpy(&value, bytes, sizeof(value));
    return value;
}

std::int16_t read_i16(const std::uint8_t* bytes) {
    std::int16_t value;
    std::memcpy(&value, bytes, sizeof(value));
    return value;
}

long long distance_squared_point_to_segment(int px, int py, int x1, int y1, int x2, int y2) {
    const long long dx = static_cast<long long>(x2) - x1;
    const long long dy = static_cast<long long>(y2) - y1;
    const long long length_squared = dx * dx + dy * dy;
    if (length_squared == 0) {
        const long long ox = static_cast<long long>(px) - x1;
        const long long oy = static_cast<long long>(py) - y1;
        return ox * ox + oy * oy;
    }

    long long t = ((static_cast<long long>(px) - x1) * dx + (static_cast<long long>(py) - y1) * dy) *
                  1000 / length_squared;
    if (t < 0) {
        t = 0;
    } else if (t > 1000) {
        t = 1000;
    }

    const long long closest_x = x1 + (dx * t) / 1000;
    const long long closest_y = y1 + (dy * t) / 1000;
    const long long ox = static_cast<long long>(px) - closest_x;
    const long long oy = static_cast<long long>(py) - closest_y;
    return ox * ox + oy * oy;
}

bool load_points(const WadLumpData& lump, std::vector<MapPoint>& points) {
    if (lump.size < 4) {
        return false;
    }

    const int count = read_i32(lump.data);
    if (count < 0 || 4 + static_cast<std::size_t>(count) * 8u > lump.size) {
        return false;
    }

    points.resize(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        const auto* record = lump.data + 4 + static_cast<std::size_t>(i) * 8u;
        const std::int16_t y = read_i16(record + 2);
        const std::int16_t x = read_i16(record + 6);
        points[static_cast<std::size_t>(i)] = {x, y};
    }
    return true;
}

int texture_index_for_special(std::int16_t special, int line_index) {
    if (special == 376 || special == 448) {
        return 33;
    }
    if (special == 64) {
        return 27;
    }
    if (special == 256) {
        return 0;
    }
    if (special == 8) {
        return 14;
    }
    return (std::abs(static_cast<int>(special)) / 16 + line_index) % 34;
}

bool load_lines(const WadLumpData& lump, std::vector<MapLine>& lines) {
    if (lump.size < 4) {
        return false;
    }

    const int count = read_i32(lump.data);
    if (count < 0 || 4 + static_cast<std::size_t>(count) * 20u > lump.size) {
        return false;
    }

    lines.resize(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        const auto* record = lump.data + 4 + static_cast<std::size_t>(i) * 20u;
        MapLine line;
        line.v1 = read_i16(record);
        line.v2 = read_i16(record + 2);
        line.special = read_i16(record + 10);
        line.texture_index = texture_index_for_special(line.special, i);
        lines[static_cast<std::size_t>(i)] = line;
    }
    return true;
}

bool load_things(const WadLumpData& lump, std::vector<MapThing>& things) {
    if (lump.size < 4) {
        return false;
    }

    const int count = read_i32(lump.data);
    if (count < 0 || 4 + static_cast<std::size_t>(count) * 16u > lump.size) {
        return false;
    }

    things.resize(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        const auto* record = lump.data + 4 + static_cast<std::size_t>(i) * 16u;
        const std::int16_t y = read_i16(record + 2);
        const std::int16_t x = read_i16(record + 6);
        const std::int16_t type = read_i16(record + 10);
        things[static_cast<std::size_t>(i)] = {x, y, type};
    }
    return true;
}

}  // namespace

bool Map::load_from_wad(const Wad& wad, Map& map) {
    const auto points_lump = wad.find_lump("M_POINTS");
    const auto lines_lump = wad.find_lump("M_LINES");
    if (!points_lump || !lines_lump) {
        std::fprintf(stderr, "M_POINTS or M_LINES lump not found\n");
        return false;
    }

    if (!load_points(wad.lump_data(*points_lump), map.points_)) {
        std::fprintf(stderr, "Failed to parse M_POINTS\n");
        return false;
    }
    if (!load_lines(wad.lump_data(*lines_lump), map.lines_)) {
        std::fprintf(stderr, "Failed to parse M_LINES\n");
        return false;
    }

    const auto things_lump = wad.find_lump("M_THINGS");
    if (things_lump) {
        load_things(wad.lump_data(*things_lump), map.things_);
    }

    int door_count = 0;
    for (const MapLine& line : map.lines_) {
        if (line.is_door()) {
            ++door_count;
        }
    }

    std::printf("Map: %d points, %d lines (%d doors), %d things\n", map.point_count(),
                map.line_count(), door_count, static_cast<int>(map.things_.size()));
    return true;
}

MapBounds Map::bounds() const {
    MapBounds bounds;
    if (points_.empty()) {
        return bounds;
    }

    bounds.min_x = bounds.max_x = points_[0].x;
    bounds.min_y = bounds.max_y = points_[0].y;

    for (const MapPoint& point : points_) {
        bounds.min_x = std::min(bounds.min_x, static_cast<int>(point.x));
        bounds.max_x = std::max(bounds.max_x, static_cast<int>(point.x));
        bounds.min_y = std::min(bounds.min_y, static_cast<int>(point.y));
        bounds.max_y = std::max(bounds.max_y, static_cast<int>(point.y));
    }
    for (const MapThing& thing : things_) {
        bounds.min_x = std::min(bounds.min_x, static_cast<int>(thing.x));
        bounds.max_x = std::max(bounds.max_x, static_cast<int>(thing.x));
        bounds.min_y = std::min(bounds.min_y, static_cast<int>(thing.y));
        bounds.max_y = std::max(bounds.max_y, static_cast<int>(thing.y));
    }
    return bounds;
}

bool Map::is_position_valid(std::int16_t x, std::int16_t y,
                            const std::vector<MapLineState>& lines) const {
    const long long radius_squared =
        static_cast<long long>(kPlayerRadius) * static_cast<long long>(kPlayerRadius);

    for (const MapLineState& state : lines) {
        if (!state.blocks()) {
            continue;
        }

        const MapPoint& a = points_[static_cast<std::size_t>(state.line.v1)];
        const MapPoint& b = points_[static_cast<std::size_t>(state.line.v2)];
        if (distance_squared_point_to_segment(x, y, a.x, a.y, b.x, b.y) <= radius_squared) {
            return false;
        }
    }
    return true;
}

void Map::try_move(std::int16_t& x, std::int16_t& y, int dx, int dy,
                   const std::vector<MapLineState>& lines) const {
    if (dx == 0 && dy == 0) {
        return;
    }

    const int target_x = static_cast<int>(x) + dx;
    const int target_y = static_cast<int>(y) + dy;

    if (is_position_valid(static_cast<std::int16_t>(target_x), static_cast<std::int16_t>(target_y),
                          lines)) {
        x = static_cast<std::int16_t>(target_x);
        y = static_cast<std::int16_t>(target_y);
        return;
    }

    if (dx != 0 &&
        is_position_valid(static_cast<std::int16_t>(static_cast<int>(x) + dx), y, lines)) {
        x = static_cast<std::int16_t>(static_cast<int>(x) + dx);
    }
    if (dy != 0 &&
        is_position_valid(x, static_cast<std::int16_t>(static_cast<int>(y) + dy), lines)) {
        y = static_cast<std::int16_t>(static_cast<int>(y) + dy);
    }
}

void Map::draw(Screen& screen, std::uint8_t line_color, std::uint8_t door_color,
               std::uint8_t point_color, std::uint8_t thing_color, std::uint8_t player_color,
               std::int16_t player_x, std::int16_t player_y,
               const std::vector<MapThingState>& things,
               const std::vector<MapLineState>& lines) const {
    if (points_.empty()) {
        return;
    }

    const MapBounds bounds = this->bounds();
    const int map_w = std::max(1, bounds.max_x - bounds.min_x);
    const int map_h = std::max(1, bounds.max_y - bounds.min_y);
    constexpr int margin = 8;
    const float scale = std::min(
        static_cast<float>(Screen::kWidth - margin * 2) / static_cast<float>(map_w),
        static_cast<float>(Screen::kHeight - margin * 2) / static_cast<float>(map_h));

    const auto to_screen = [&](std::int16_t x, std::int16_t y) {
        const int sx =
            margin + static_cast<int>((static_cast<float>(x - bounds.min_x) + 0.5f) * scale);
        const int sy = Screen::kHeight - margin -
                       static_cast<int>((static_cast<float>(y - bounds.min_y) + 0.5f) * scale);
        return std::pair<int, int>{sx, sy};
    };

    for (const MapLineState& state : lines) {
        if (state.line.v1 < 0 || state.line.v2 < 0 || state.line.v1 >= point_count() ||
            state.line.v2 >= point_count()) {
            continue;
        }
        const MapPoint& a = points_[static_cast<std::size_t>(state.line.v1)];
        const MapPoint& b = points_[static_cast<std::size_t>(state.line.v2)];
        const auto [x0, y0] = to_screen(a.x, a.y);
        const auto [x1, y1] = to_screen(b.x, b.y);
        const std::uint8_t color =
            state.line.is_door() ? (state.door_open ? door_color : line_color) : line_color;
        screen.draw_line(x0, y0, x1, y1, color);
    }

    for (const MapPoint& point : points_) {
        const auto [sx, sy] = to_screen(point.x, point.y);
        screen.fill_rect(sx - 1, sy - 1, 3, 3, point_color);
    }

    for (const MapThingState& state : things) {
        if (state.thing.type == 2) {
            continue;
        }
        const auto [sx, sy] = to_screen(state.thing.x, state.thing.y);
        std::uint8_t color = thing_color;
        switch (state.thing.type) {
            case 0:
                color = state.activated ? static_cast<std::uint8_t>(180) : static_cast<std::uint8_t>(215);
                break;
            case 1:
                color = state.activated ? static_cast<std::uint8_t>(231) : static_cast<std::uint8_t>(112);
                break;
            case 3:
                color = state.activated ? static_cast<std::uint8_t>(128) : static_cast<std::uint8_t>(247);
                break;
            default:
                break;
        }
        screen.fill_rect(sx - 2, sy - 2, 5, 5, color);
    }

    const auto [px, py] = to_screen(player_x, player_y);
    screen.fill_rect(px - 3, py - 3, 7, 7, player_color);
}
