#include "map.hpp"

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
        lines[static_cast<std::size_t>(i)] = {read_i16(record), read_i16(record + 2)};
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

    std::printf("Map: %d points, %d lines, %d things\n", map.point_count(), map.line_count(),
                static_cast<int>(map.things_.size()));
    return true;
}

void Map::draw(Screen& screen, std::uint8_t line_color, std::uint8_t point_color,
               std::uint8_t thing_color) const {
    if (points_.empty()) {
        return;
    }

    int min_x = points_[0].x;
    int max_x = points_[0].x;
    int min_y = points_[0].y;
    int max_y = points_[0].y;

    for (const MapPoint& point : points_) {
        min_x = std::min(min_x, static_cast<int>(point.x));
        max_x = std::max(max_x, static_cast<int>(point.x));
        min_y = std::min(min_y, static_cast<int>(point.y));
        max_y = std::max(max_y, static_cast<int>(point.y));
    }
    for (const MapThing& thing : things_) {
        min_x = std::min(min_x, static_cast<int>(thing.x));
        max_x = std::max(max_x, static_cast<int>(thing.x));
        min_y = std::min(min_y, static_cast<int>(thing.y));
        max_y = std::max(max_y, static_cast<int>(thing.y));
    }

    const int map_w = std::max(1, max_x - min_x);
    const int map_h = std::max(1, max_y - min_y);
    constexpr int margin = 8;
    const float scale = std::min(
        static_cast<float>(Screen::kWidth - margin * 2) / static_cast<float>(map_w),
        static_cast<float>(Screen::kHeight - margin * 2) / static_cast<float>(map_h));

    const auto to_screen = [&](std::int16_t x, std::int16_t y) {
        const int sx =
            margin + static_cast<int>((static_cast<float>(x - min_x) + 0.5f) * scale);
        const int sy = Screen::kHeight - margin -
                       static_cast<int>((static_cast<float>(y - min_y) + 0.5f) * scale);
        return std::pair<int, int>{sx, sy};
    };

    for (const MapLine& line : lines_) {
        if (line.v1 < 0 || line.v2 < 0 || line.v1 >= point_count() || line.v2 >= point_count()) {
            continue;
        }
        const MapPoint& a = points_[static_cast<std::size_t>(line.v1)];
        const MapPoint& b = points_[static_cast<std::size_t>(line.v2)];
        const auto [x0, y0] = to_screen(a.x, a.y);
        const auto [x1, y1] = to_screen(b.x, b.y);
        screen.draw_line(x0, y0, x1, y1, line_color);
    }

    for (const MapPoint& point : points_) {
        const auto [sx, sy] = to_screen(point.x, point.y);
        screen.fill_rect(sx - 1, sy - 1, 3, 3, point_color);
    }

    for (const MapThing& thing : things_) {
        const auto [sx, sy] = to_screen(thing.x, thing.y);
        screen.fill_rect(sx - 2, sy - 2, 5, 5, thing_color);
    }
}
