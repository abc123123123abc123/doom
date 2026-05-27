#include "render3d.hpp"

#include "patch.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace {

constexpr int kHorizon = Screen::kHeight / 2;
constexpr int kViewHeight = 41;
constexpr float kFocalLength = 160.0f;
constexpr float kMaxDistance = 1024.0f;
constexpr float kPi = 3.14159265f;

const char* kWallTextures[] = {
    "WALLA6_1", "WALLA6_2", "WALLA6_3", "WALLA6_4", "WALLA6_5", "WALLA6_6", "WALLA6_7",
    "WALLA7_1", "WALLA7_2", "WALLA7_3", "WALLA7_4", "WALLA7_5", "WALLA7_6", "WALLA7_7",
    "WALLA8_1", "WALLA8_2", "WALLA8_3", "WALLA8_4", "WALLA8_5", "WALLA8_6", "WALLA8_7",
    "WALLBA_1", "WALLBA_2", "WALLBA_3", "WALLBA_4", "WALLBA_5", "WALLBA_6",
    "WALLBB_1", "WALLBB_2", "WALLBB_3", "WALLBB_4", "WALLBB_5", "WALLBB_6",
    "WALL6_1",
};
constexpr int kWallTextureCount = static_cast<int>(sizeof(kWallTextures) / sizeof(kWallTextures[0]));

}  // namespace

bool Render3D::trace_ray(const Map& map, const std::vector<MapLineState>& lines, float origin_x,
                         float origin_y, float dir_x, float dir_y, Hit& hit) const {
    hit.distance = 0.0f;
    hit.line_index = -1;
    hit.wall_offset = 0.0f;

    float closest = kMaxDistance;
    int closest_line = -1;
    float closest_offset = 0.0f;

    for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
        const MapLineState& state = lines[static_cast<std::size_t>(i)];
        if (!state.blocks()) {
            continue;
        }

        const MapLine& line = state.line;
        if (line.v1 < 0 || line.v2 < 0 || line.v1 >= map.point_count() ||
            line.v2 >= map.point_count()) {
            continue;
        }

        const MapPoint& a = map.points()[static_cast<std::size_t>(line.v1)];
        const MapPoint& b = map.points()[static_cast<std::size_t>(line.v2)];

        const float x1 = static_cast<float>(a.x);
        const float y1 = static_cast<float>(a.y);
        const float x2 = static_cast<float>(b.x);
        const float y2 = static_cast<float>(b.y);

        const float seg_x = x2 - x1;
        const float seg_y = y2 - y1;
        const float det = dir_x * seg_y - dir_y * seg_x;
        if (std::fabs(det) < 0.0001f) {
            continue;
        }

        const float rx = x1 - origin_x;
        const float ry = y1 - origin_y;
        const float t = (rx * seg_y - ry * seg_x) / det;
        const float u = (rx * dir_y - ry * dir_x) / det;
        if (t <= 0.5f || u < 0.0f || u > 1.0f) {
            continue;
        }

        if (t < closest) {
            closest = t;
            closest_line = i;
            closest_offset = u * std::sqrt(seg_x * seg_x + seg_y * seg_y);
        }
    }

    if (closest_line < 0) {
        return false;
    }

    hit.distance = closest;
    hit.line_index = closest_line;
    hit.wall_offset = closest_offset;
    return true;
}

std::uint8_t Render3D::shade_for_distance(float distance) const {
    const int shade = static_cast<int>(distance / 32.0f);
    return static_cast<std::uint8_t>(std::min(31, shade));
}

const char* Render3D::texture_name_for_line(int line_index) const {
    if (line_index < 0) {
        return kWallTextures[0];
    }
    return kWallTextures[line_index % kWallTextureCount];
}

void Render3D::render(Screen& screen, const Wad& wad, const Map& map,
                      const std::vector<MapLineState>& lines, float player_x, float player_y,
                      float player_angle) const {
    screen.clear(25);

    for (int x = 0; x < Screen::kWidth; ++x) {
        for (int y = 0; y < kHorizon; ++y) {
            screen.put_pixel(x, y, 22);
        }
        for (int y = kHorizon; y < Screen::kHeight; ++y) {
            screen.put_pixel(x, y, 25);
        }
    }

    static std::unordered_map<std::string, std::vector<std::uint8_t>> lump_cache;
    static std::unordered_map<std::string, PatchInfo> info_cache;
    static std::unordered_map<std::string, std::vector<std::vector<std::uint8_t>>> column_cache;

    const float plane_x0 = std::cos(player_angle + kPi / 2.0f);
    const float plane_y0 = std::sin(player_angle + kPi / 2.0f);

    for (int x = 0; x < Screen::kWidth; ++x) {
        const float camera_x = (2.0f * static_cast<float>(x) / static_cast<float>(Screen::kWidth)) -
                               1.0f;
        const float ray_dir_x = std::cos(player_angle) + plane_x0 * camera_x * 0.66f;
        const float ray_dir_y = std::sin(player_angle) + plane_y0 * camera_x * 0.66f;

        const float inv_length =
            1.0f / std::sqrt(ray_dir_x * ray_dir_x + ray_dir_y * ray_dir_y);
        const float dir_x = ray_dir_x * inv_length;
        const float dir_y = ray_dir_y * inv_length;

        Hit hit;
        if (!trace_ray(map, lines, player_x, player_y, dir_x, dir_y, hit)) {
            continue;
        }

        const int wall_height = static_cast<int>((kViewHeight * kFocalLength) / hit.distance);
        const int top = std::max(0, kHorizon - wall_height / 2);
        const int bottom = std::min(Screen::kHeight, kHorizon + wall_height / 2);

        for (int y = 0; y < top; ++y) {
            screen.put_pixel(x, y, 22);
        }
        for (int y = bottom; y < Screen::kHeight; ++y) {
            screen.put_pixel(x, y, 25);
        }

        const char* texture_name = texture_name_for_line(hit.line_index);
        std::string key(texture_name);

        if (lump_cache.find(key) == lump_cache.end()) {
            const auto lump_index = wad.find_lump(texture_name);
            if (!lump_index) {
                screen.draw_column(x, top, bottom, static_cast<std::uint8_t>(176 + shade_for_distance(hit.distance)));
                continue;
            }

            const WadLumpData lump_data = wad.lump_data(*lump_index);
            PatchInfo info;
            if (!patch_info(lump_data, info)) {
                screen.draw_column(x, top, bottom, static_cast<std::uint8_t>(176 + shade_for_distance(hit.distance)));
                continue;
            }

            std::vector<std::uint8_t> stored(lump_data.data, lump_data.data + lump_data.size);
            lump_cache[key] = std::move(stored);
            info_cache[key] = info;

            WadLumpData stored_lump{lump_cache[key].data(), lump_cache[key].size()};
            std::vector<std::vector<std::uint8_t>> columns(static_cast<std::size_t>(info.width));
            for (int col = 0; col < info.width; ++col) {
                patch_column_pixels(stored_lump, col, columns[static_cast<std::size_t>(col)]);
            }
            column_cache[key] = std::move(columns);
        }

        const PatchInfo& info = info_cache[key];
        const int tex_column =
            static_cast<int>(hit.wall_offset) % std::max(1, info.width);
        const std::vector<std::uint8_t>& column_pixels =
            column_cache[key][static_cast<std::size_t>(tex_column)];

        screen.draw_column_scaled(x, top, bottom, column_pixels.data(), info.height,
                                 static_cast<std::uint8_t>(176 + shade_for_distance(hit.distance)));
    }
}
