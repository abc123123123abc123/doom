#include "render3d.hpp"

#include "patch.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

constexpr int kHorizon = Screen::kHeight / 2;
constexpr int kViewHeight = 41;
constexpr float kWorldWallHeight = 96.0f;
constexpr float kMinProjectedPerpDistance = 8.0f;
constexpr float kCameraPlaneScale = 0.66f;
constexpr float kFocalLength =
    static_cast<float>(Screen::kWidth) / (2.0f * kCameraPlaneScale);
constexpr float kMaxDistance = 1024.0f;
constexpr float kFloorCastMax = 640.0f;
constexpr float kPi = 3.14159265f;
constexpr int kFlatSize = 64 * 64;
constexpr int kMaxSpriteDrawSize = Screen::kHeight + 48;
constexpr std::uint8_t kSkyColor = 22;
constexpr std::uint8_t kFloorVoidColor = 25;

const char* kWallTextures[] = {
    "WALLA6_1", "WALLA6_2", "WALLA6_3", "WALLA6_4", "WALLA6_5", "WALLA6_6", "WALLA6_7",
    "WALLA7_1", "WALLA7_2", "WALLA7_3", "WALLA7_4", "WALLA7_5", "WALLA7_6", "WALLA7_7",
    "WALLA8_1", "WALLA8_2", "WALLA8_3", "WALLA8_4", "WALLA8_5", "WALLA8_6", "WALLA8_7",
    "WALLBA_1", "WALLBA_2", "WALLBA_3", "WALLBA_4", "WALLBA_5", "WALLBA_6",
    "WALLBB_1", "WALLBB_2", "WALLBB_3", "WALLBB_4", "WALLBB_5", "WALLBB_6",
    "WALL6_1",
};
constexpr int kWallTextureCount = static_cast<int>(sizeof(kWallTextures) / sizeof(kWallTextures[0]));
constexpr const char* kFallbackWallTexture = "WALLA6_1";
constexpr const char* kBoundaryWallTexture = "WALLBB_3";

struct PatchColumns {
    PatchInfo info;
    std::vector<std::uint8_t> lump;
    std::vector<std::vector<std::uint8_t>> columns;
};

bool load_flat(const Wad& wad, const char* name, std::array<std::uint8_t, kFlatSize>& out) {
    const auto lump_index = wad.find_lump(name);
    if (!lump_index) {
        return false;
    }
    const WadLumpData lump = wad.lump_data(*lump_index);
    if (lump.size < static_cast<std::size_t>(kFlatSize)) {
        return false;
    }
    std::memcpy(out.data(), lump.data, kFlatSize);
    return true;
}

std::uint8_t sample_flat(const std::array<std::uint8_t, kFlatSize>& flat, float world_x,
                         float world_y) {
    const int tx = static_cast<int>(world_x) & 63;
    const int ty = static_cast<int>(world_y) & 63;
    return flat[static_cast<std::size_t>(ty * 64 + tx)];
}

void put_shaded(Screen& screen, int x, int y, std::uint8_t color, int light,
                const Palette& palette) {
    screen.put_pixel(x, y, palette.map_index(color, light));
}

PatchColumns* ensure_patch_columns(const Wad& wad,
                                   std::unordered_map<std::string, PatchColumns>& cache,
                                   const char* lump_name) {
    const std::string key(lump_name);
    const auto found = cache.find(key);
    if (found != cache.end()) {
        return &found->second;
    }

    const auto lump_index = wad.find_lump(lump_name);
    if (!lump_index) {
        if (std::strcmp(lump_name, kFallbackWallTexture) != 0) {
            return ensure_patch_columns(wad, cache, kFallbackWallTexture);
        }
        return nullptr;
    }

    const WadLumpData lump_data = wad.lump_data(*lump_index);
    PatchInfo info;
    if (!patch_info(lump_data, info)) {
        if (std::strcmp(lump_name, kFallbackWallTexture) != 0) {
            return ensure_patch_columns(wad, cache, kFallbackWallTexture);
        }
        return nullptr;
    }

    PatchColumns entry;
    entry.info = info;
    entry.lump.assign(lump_data.data, lump_data.data + lump_data.size);
    entry.columns.resize(static_cast<std::size_t>(info.width));

    WadLumpData stored_lump{entry.lump.data(), entry.lump.size()};
    for (int col = 0; col < info.width; ++col) {
        if (!patch_column_pixels_opaque(stored_lump, col,
                                        entry.columns[static_cast<std::size_t>(col)])) {
            if (std::strcmp(lump_name, kFallbackWallTexture) != 0) {
                return ensure_patch_columns(wad, cache, kFallbackWallTexture);
            }
            return nullptr;
        }
    }

    const auto inserted = cache.emplace(key, std::move(entry));
    return &inserted.first->second;
}

const char* sprite_lump_for_thing(std::int16_t type, int gametic) {
    static const char* kImpFrames[] = {"TROOA1", "TROOB1", "TROOC1", "TROOD1"};
    static const char* kDemonFrames[] = {"SARGA1", "SARGB1", "SARGC1", "SARGD1"};
    static const char* kBaronFrames[] = {"BOSSA1", "BOSSB1", "BOSSC1", "BOSSD1"};
    const int frame = (gametic / 6) % 4;
    switch (type) {
        case 0:
            return kImpFrames[frame];
        case 1:
            return kDemonFrames[frame];
        case 3:
            return kBaronFrames[frame];
        default:
            return kImpFrames[frame];
    }
}

struct SpriteDraw {
    float depth = 0.0f;
    float screen_x = 0.0f;
    const char* lump_name = nullptr;
};

void draw_sprite(Screen& screen, const Wad& wad, std::unordered_map<std::string, PatchColumns>& cache,
                 const Palette& palette, const char* lump_name, float screen_x, float depth,
                 const std::array<float, Screen::kWidth>& z_buffer) {
    PatchColumns* patch = ensure_patch_columns(wad, cache, lump_name);
    if (patch == nullptr || patch->info.width <= 0 || patch->info.height <= 0) {
        return;
    }

    const float safe_depth = std::max(8.0f, depth);
    const float scale = kFocalLength / safe_depth;

    int draw_height = static_cast<int>(patch->info.height * scale);
    int draw_width = static_cast<int>(patch->info.width * scale);
    draw_height = std::clamp(draw_height, 1, kMaxSpriteDrawSize);
    draw_width = std::clamp(draw_width, 1, kMaxSpriteDrawSize);

    const int anchor_y = kHorizon;
    const int y_top = anchor_y - static_cast<int>(patch->info.topoffset * scale);
    const int y_bottom = y_top + draw_height;
    const int start_x =
        static_cast<int>(screen_x) - static_cast<int>(patch->info.leftoffset * scale);
    const int light = std::min(31, static_cast<int>(safe_depth / 32.0f));

    for (int column = 0; column < draw_width; ++column) {
        const int screen_column = start_x + column;
        if (screen_column < 0 || screen_column >= Screen::kWidth) {
            continue;
        }
        if (safe_depth >= z_buffer[static_cast<std::size_t>(screen_column)]) {
            continue;
        }

        const int tex_column = (column * patch->info.width) / std::max(1, draw_width);
        const std::vector<std::uint8_t>& pixels =
            patch->columns[static_cast<std::size_t>(std::min(tex_column, patch->info.width - 1))];
        screen.draw_patch_column_shaded(screen_column, y_top, y_bottom, pixels.data(),
                                        patch->info.height, light, palette);
    }
}

bool trace_segment(float origin_x, float origin_y, float dir_x, float dir_y, float x1, float y1,
                   float x2, float y2, float& closest, float& closest_offset, float& closest_u) {
    const float seg_x = x2 - x1;
    const float seg_y = y2 - y1;
    const float det = dir_x * seg_y - dir_y * seg_x;
    if (std::fabs(det) < 0.0001f) {
        return false;
    }

    const float rx = x1 - origin_x;
    const float ry = y1 - origin_y;
    const float t = (rx * seg_y - ry * seg_x) / det;
    const float u = (rx * dir_y - ry * dir_x) / det;
    if (t <= 0.5f || u < 0.0f || u > 1.0f) {
        return false;
    }

    if (t < closest) {
        closest = t;
        const float seg_len = std::sqrt(seg_x * seg_x + seg_y * seg_y);
        const float hit_x = origin_x + dir_x * t;
        const float hit_y = origin_y + dir_y * t;
        closest_offset = ((hit_x - x1) * seg_x + (hit_y - y1) * seg_y) / seg_len;
        closest_u = u;
        return true;
    }
    return false;
}

}  // namespace

bool Render3D::trace_ray(const Map& map, const std::vector<MapLineState>& lines, float origin_x,
                         float origin_y, float dir_x, float dir_y, Hit& hit) const {
    hit.distance = 0.0f;
    hit.line_index = -1;
    hit.wall_offset = 0.0f;
    hit.wall_u = 0.0f;
    hit.is_boundary = false;

    float closest = kMaxDistance;
    int closest_line = -1;
    float closest_offset = 0.0f;
    float closest_u = 0.0f;

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

        if (trace_segment(origin_x, origin_y, dir_x, dir_y, static_cast<float>(a.x),
                          static_cast<float>(a.y), static_cast<float>(b.x),
                          static_cast<float>(b.y), closest, closest_offset, closest_u)) {
            closest_line = i;
        }
    }

    const MapBounds bounds = map.bounds();
    constexpr int kMargin = 64;
    const float min_x = static_cast<float>(bounds.min_x - kMargin);
    const float max_x = static_cast<float>(bounds.max_x + kMargin);
    const float min_y = static_cast<float>(bounds.min_y - kMargin);
    const float max_y = static_cast<float>(bounds.max_y + kMargin);

    float boundary_distance = closest;
    float boundary_offset = closest_offset;
    float boundary_u = closest_u;
    const float boundary_before = boundary_distance;

    trace_segment(origin_x, origin_y, dir_x, dir_y, min_x, min_y, min_x, max_y, boundary_distance,
                  boundary_offset, boundary_u);
    trace_segment(origin_x, origin_y, dir_x, dir_y, max_x, min_y, max_x, max_y, boundary_distance,
                  boundary_offset, boundary_u);
    trace_segment(origin_x, origin_y, dir_x, dir_y, min_x, min_y, max_x, min_y, boundary_distance,
                  boundary_offset, boundary_u);
    trace_segment(origin_x, origin_y, dir_x, dir_y, min_x, max_y, max_x, max_y, boundary_distance,
                  boundary_offset, boundary_u);

    const bool hit_boundary = boundary_distance < boundary_before;

    if (closest_line < 0 && !hit_boundary) {
        return false;
    }

    if (hit_boundary && (closest_line < 0 || boundary_distance <= closest)) {
        hit.distance = boundary_distance;
        hit.line_index = -1;
        hit.wall_offset = boundary_offset;
        hit.wall_u = boundary_u;
        hit.is_boundary = true;
        return true;
    }

    hit.distance = closest;
    hit.line_index = closest_line;
    hit.wall_offset = closest_offset;
    hit.wall_u = closest_u;
    return true;
}

int Render3D::shade_for_distance(float distance) const {
    return std::min(31, static_cast<int>(distance / 32.0f));
}

const char* Render3D::texture_name_for_line(const MapLine& line) const {
    const int index = line.texture_index % kWallTextureCount;
    if (index < 0) {
        return kWallTextures[0];
    }
    return kWallTextures[index];
}

void Render3D::render(Screen& screen, const Wad& wad, const Map& map,
                      const std::vector<MapLineState>& lines,
                      const std::vector<MapThingState>& things, const Palette& palette,
                      float player_x, float player_y, float player_angle, int wall_cycle,
                      int ceiling_cycle, int floor_cycle, int light_cycle, int gametic) const {
    static std::unordered_map<std::string, PatchColumns> patch_cache;
    static std::array<std::uint8_t, kFlatSize> floor_flat{};
    static std::array<std::uint8_t, kFlatSize> ceiling_flat{};
    static bool flats_loaded = false;

    if (!flats_loaded) {
        if (!load_flat(wad, "FLAT1", floor_flat)) {
            floor_flat.fill(kFloorVoidColor);
        }
        if (!load_flat(wad, "FLAT5", ceiling_flat)) {
            ceiling_flat.fill(kSkyColor);
        }
        flats_loaded = true;
    }

    std::array<std::uint8_t, kFlatSize> active_floor = floor_flat;
    std::array<std::uint8_t, kFlatSize> active_ceiling = ceiling_flat;
    {
        const int f = ((floor_cycle % 8) + 8) % 8 + 1;
        const int c = ((ceiling_cycle % 8) + 8) % 8 + 1;
        const std::string floor_name = "FLAT" + std::to_string(f);
        const std::string ceil_name = "FLAT" + std::to_string(c);
        load_flat(wad, floor_name.c_str(), active_floor);
        load_flat(wad, ceil_name.c_str(), active_ceiling);
    }

    screen.clear(palette.map_index(kFloorVoidColor, 0));

    const float plane_x = std::cos(player_angle + kPi / 2.0f) * kCameraPlaneScale;
    const float plane_y = std::sin(player_angle + kPi / 2.0f) * kCameraPlaneScale;
    const float dir_x_base = std::cos(player_angle);
    const float dir_y_base = std::sin(player_angle);
    const float right_x = std::cos(player_angle + kPi / 2.0f);
    const float right_y = std::sin(player_angle + kPi / 2.0f);

    std::array<float, Screen::kWidth> z_buffer{};
    z_buffer.fill(kMaxDistance);

    for (int x = 0; x < Screen::kWidth; ++x) {
        const float camera_x =
            (2.0f * static_cast<float>(x) / static_cast<float>(Screen::kWidth)) - 1.0f;
        const float ray_dir_x = dir_x_base + plane_x * camera_x;
        const float ray_dir_y = dir_y_base + plane_y * camera_x;
        const float ray_length =
            std::sqrt(ray_dir_x * ray_dir_x + ray_dir_y * ray_dir_y);
        const float dir_x = ray_dir_x / ray_length;
        const float dir_y = ray_dir_y / ray_length;
        const float ray_camera_dot =
            (ray_dir_x * dir_x_base + ray_dir_y * dir_y_base) / ray_length;

        Hit hit;
        int wall_top = 0;
        int wall_bottom = Screen::kHeight;
        float perp_distance = kMaxDistance;
        float projected_perp_distance = kMaxDistance;
        const bool has_wall = trace_ray(map, lines, player_x, player_y, dir_x, dir_y, hit);
        if (has_wall) {
            perp_distance = hit.distance * ray_camera_dot;
            z_buffer[static_cast<std::size_t>(x)] = perp_distance;
            projected_perp_distance = std::max(kMinProjectedPerpDistance, perp_distance);
            const int wall_screen_height = static_cast<int>(
                (kWorldWallHeight * kFocalLength) / projected_perp_distance);
            const int horizon = kHorizon + (ceiling_cycle - 4) * 3;
            wall_top = horizon - wall_screen_height / 2;
            wall_bottom = horizon + wall_screen_height / 2;
        }

        const int clipped_wall_top = std::clamp(wall_top, 0, Screen::kHeight);
        const int clipped_wall_bottom = std::clamp(wall_bottom, 0, Screen::kHeight);

        const float depth_limit = has_wall ? perp_distance : kFloorCastMax;

        for (int y = 0; y < clipped_wall_top; ++y) {
            const int p = kHorizon - y;
            if (p <= 0) {
                continue;
            }
            const float perp_row_dist = (kViewHeight * kFocalLength) / static_cast<float>(p);
            if (perp_row_dist > depth_limit) {
                put_shaded(screen, x, y, kSkyColor, 31, palette);
                continue;
            }
            const float world_x =
                player_x + dir_x_base * perp_row_dist +
                right_x * perp_row_dist * camera_x * kCameraPlaneScale;
            const float world_y =
                player_y + dir_y_base * perp_row_dist +
                right_y * perp_row_dist * camera_x * kCameraPlaneScale;
            const std::uint8_t color = sample_flat(active_ceiling, world_x, world_y);
            const int light = std::clamp(shade_for_distance(perp_row_dist) + (light_cycle - 5), 0, 31);
            put_shaded(screen, x, y, color, light, palette);
        }

        for (int y = clipped_wall_bottom; y < Screen::kHeight; ++y) {
            const int p = y - kHorizon;
            if (p <= 0) {
                continue;
            }
            const float perp_row_dist = (kViewHeight * kFocalLength) / static_cast<float>(p);
            if (perp_row_dist > depth_limit) {
                put_shaded(screen, x, y, kFloorVoidColor, 31, palette);
                continue;
            }
            const float world_x =
                player_x + dir_x_base * perp_row_dist +
                right_x * perp_row_dist * camera_x * kCameraPlaneScale;
            const float world_y =
                player_y + dir_y_base * perp_row_dist +
                right_y * perp_row_dist * camera_x * kCameraPlaneScale;
            const std::uint8_t color = sample_flat(active_floor, world_x, world_y);
            const int light = std::clamp(shade_for_distance(perp_row_dist) + (light_cycle - 5), 0, 31);
            put_shaded(screen, x, y, color, light, palette);
        }

        if (!has_wall) {
            continue;
        }

        const char* texture_name = kBoundaryWallTexture;
        if (!hit.is_boundary) {
            const MapLine& hit_line = lines[static_cast<std::size_t>(hit.line_index)].line;
            const std::string& map_texture = map.patch_name_for_index(hit_line.texture_index);
            if (map_texture.empty()) {
                texture_name = texture_name_for_line(hit_line);
            } else {
                texture_name = map_texture.c_str();
            }
            if (!map_texture.empty()) {
                const int idx = (hit_line.texture_index + wall_cycle) %
                                std::max(1, static_cast<int>(map.patch_names().size()));
                const std::string& cycled = map.patch_name_for_index(idx);
                if (!cycled.empty()) {
                    texture_name = cycled.c_str();
                }
            }
        }
        PatchColumns* patch = ensure_patch_columns(wad, patch_cache, texture_name);
        const int light = std::clamp(shade_for_distance(perp_distance) + (light_cycle - 5), 0, 31);

        if (patch == nullptr) {
            screen.draw_column(x, wall_top, wall_bottom, palette.map_index(176, light));
            continue;
        }

        int tex_column = 0;
        if (patch->info.width > 1) {
            float u = std::clamp(hit.wall_u, 0.0f, 0.9999f);
            if (!hit.is_boundary) {
                const MapLine& hit_line = lines[static_cast<std::size_t>(hit.line_index)].line;
                u += static_cast<float>(hit_line.texture_u_offset) / 32.0f;
                u += map.texture_u_bias_for_line(hit_line);
                u -= std::floor(u);
            }
            tex_column = static_cast<int>(u * static_cast<float>(patch->info.width));
            tex_column = std::clamp(tex_column, 0, patch->info.width - 1);
        }
        const std::vector<std::uint8_t>& column_pixels =
            patch->columns[static_cast<std::size_t>(tex_column)];
        screen.draw_wall_column_shaded(x, wall_top, wall_bottom, column_pixels.data(),
                                       patch->info.height, light, palette);
    }

    std::vector<SpriteDraw> sprites;
    sprites.reserve(things.size());
    const float transform_det = plane_x * dir_y_base - plane_y * dir_x_base;

    for (const MapThingState& state : things) {
        if (state.thing.type == 2) {
            continue;
        }

        const float dx = static_cast<float>(state.thing.x) - player_x;
        const float dy = static_cast<float>(state.thing.y) - player_y;
        const float transform_y = (dir_y_base * dx - dir_x_base * dy) / transform_det;
        if (transform_y <= 0.5f) {
            continue;
        }

        const float transform_x = (-plane_y * dx + plane_x * dy) / transform_det;
        const float inv_z = 1.0f / transform_y;
        const float sprite_screen_x =
            (static_cast<float>(Screen::kWidth) / 2.0f) * (1.0f + transform_x * inv_z);

        SpriteDraw draw;
        draw.depth = transform_y;
        draw.screen_x = sprite_screen_x;
        draw.lump_name = sprite_lump_for_thing(state.thing.type, gametic);
        sprites.push_back(draw);
    }

    std::sort(sprites.begin(), sprites.end(),
              [](const SpriteDraw& a, const SpriteDraw& b) { return a.depth > b.depth; });

    for (const SpriteDraw& sprite : sprites) {
        draw_sprite(screen, wad, patch_cache, palette, sprite.lump_name, sprite.screen_x,
                    sprite.depth, z_buffer);
    }
}
