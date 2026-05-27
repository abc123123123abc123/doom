#include "game.hpp"

#include "patch.hpp"

#include <cmath>
#include <cstdio>

namespace {

const char* kTrooFrames[] = {
    "TROOA1", "TROOA2", "TROOA3", "TROOA4",
    "TROOA5", "TROOA6", "TROOA7", "TROOA8",
};
constexpr int kTrooFrameCount = static_cast<int>(sizeof(kTrooFrames) / sizeof(kTrooFrames[0]));

}  // namespace

void Game::init_from_map(const Map& map) {
    things_.clear();
    things_.reserve(map.things().size());
    for (const MapThing& thing : map.things()) {
        things_.push_back({thing, false});
    }

    lines_.clear();
    lines_.reserve(map.lines().size());
    for (const MapLine& line : map.lines()) {
        lines_.push_back({line, false});
    }

    player_x_ = 128;
    player_y_ = 256;
    for (const MapThingState& state : things_) {
        if (state.thing.type == 2) {
            player_x_ = state.thing.x;
            player_y_ = state.thing.y;
            break;
        }
    }
}

void Game::set_view(View view) {
    if (this->view == view) {
        return;
    }
    this->view = view;
    needs_redraw = true;
}

void Game::toggle_doors_near(const Map& map, std::int16_t x, std::int16_t y) {
    const int link_distance_squared = kDoorLinkDistance * kDoorLinkDistance;
    bool open_doors = false;

    for (MapLineState& state : lines_) {
        if (!state.line.is_door()) {
            continue;
        }

        const MapPoint& a = map.points()[static_cast<std::size_t>(state.line.v1)];
        const MapPoint& b = map.points()[static_cast<std::size_t>(state.line.v2)];
        const int mid_x = (static_cast<int>(a.x) + static_cast<int>(b.x)) / 2;
        const int mid_y = (static_cast<int>(a.y) + static_cast<int>(b.y)) / 2;
        const int dx = mid_x - static_cast<int>(x);
        const int dy = mid_y - static_cast<int>(y);
        if (dx * dx + dy * dy > link_distance_squared) {
            continue;
        }

        if (!state.door_open) {
            open_doors = true;
        }
    }

    bool toggled_any = false;
    for (MapLineState& state : lines_) {
        if (!state.line.is_door()) {
            continue;
        }

        const MapPoint& a = map.points()[static_cast<std::size_t>(state.line.v1)];
        const MapPoint& b = map.points()[static_cast<std::size_t>(state.line.v2)];
        const int mid_x = (static_cast<int>(a.x) + static_cast<int>(b.x)) / 2;
        const int mid_y = (static_cast<int>(a.y) + static_cast<int>(b.y)) / 2;
        const int dx = mid_x - static_cast<int>(x);
        const int dy = mid_y - static_cast<int>(y);
        if (dx * dx + dy * dy <= link_distance_squared) {
            state.door_open = open_doors;
            toggled_any = true;
        }
    }

    if (!toggled_any) {
        for (MapLineState& state : lines_) {
            if (state.line.is_door()) {
                state.door_open = open_doors;
            }
        }
    }
}

void Game::open_all_doors() {
    for (MapLineState& state : lines_) {
        if (state.line.is_door()) {
            state.door_open = true;
        }
    }
}

void Game::try_use_nearby_thing(const Map& map) {
    int best_index = -1;
    int best_distance_squared = kUseDistance * kUseDistance + 1;

    for (int i = 0; i < static_cast<int>(things_.size()); ++i) {
        const MapThing& thing = things_[static_cast<std::size_t>(i)].thing;
        if (thing.type == 2) {
            continue;
        }
        const int dx = static_cast<int>(thing.x) - static_cast<int>(player_x_);
        const int dy = static_cast<int>(thing.y) - static_cast<int>(player_y_);
        const int distance_squared = dx * dx + dy * dy;
        if (distance_squared <= best_distance_squared) {
            best_distance_squared = distance_squared;
            best_index = i;
        }
    }

    if (best_index < 0) {
        return;
    }

    MapThingState& state = things_[static_cast<std::size_t>(best_index)];
    const int type = state.thing.type;

    switch (type) {
        case 0:
            toggle_doors_near(map, state.thing.x, state.thing.y);
            state.activated = !state.activated;
            std::printf("Door switch -> %s\n", state.activated ? "open" : "closed");
            break;
        case 1:
            state.activated = !state.activated;
            std::printf("Light switch -> %s\n", state.activated ? "on" : "off");
            break;
        case 2:
            std::printf("Player start (no action)\n");
            return;
        case 3:
            open_all_doors();
            state.activated = true;
            std::printf("Trigger -> all doors open\n");
            break;
        default:
            state.activated = !state.activated;
            std::printf("Thing %d type %d -> %s\n", best_index, type,
                        state.activated ? "on" : "off");
            break;
    }

    needs_redraw = true;
}

void Game::run_map_tic(const GameInput& input, const Map& map) {
    int dx = 0;
    int dy = 0;
    if (input.move_east) {
        dx += kPlayerSpeed;
    }
    if (input.move_west) {
        dx -= kPlayerSpeed;
    }
    if (input.move_north) {
        dy += kPlayerSpeed;
    }
    if (input.move_south) {
        dy -= kPlayerSpeed;
    }

    if (dx != 0 || dy != 0) {
        const std::int16_t before_x = player_x_;
        const std::int16_t before_y = player_y_;
        map.try_move(player_x_, player_y_, dx, dy, lines_);
        if (player_x_ != before_x || player_y_ != before_y) {
            needs_redraw = true;
        }
    }

    if (input.use) {
        try_use_nearby_thing(map);
    }
}

void Game::run_world_tic(const GameInput& input, const Map& map) {
    bool changed = false;

    if (input.turn_left) {
        player_angle_ -= kTurnSpeed;
        changed = true;
    }
    if (input.turn_right) {
        player_angle_ += kTurnSpeed;
        changed = true;
    }

    const float speed = static_cast<float>(kPlayerSpeed);
    float dx = 0.0f;
    float dy = 0.0f;
    if (input.move_north) {
        dx += std::cos(player_angle_) * speed;
        dy += std::sin(player_angle_) * speed;
    }
    if (input.move_south) {
        dx -= std::cos(player_angle_) * speed;
        dy -= std::sin(player_angle_) * speed;
    }
    if (input.move_west) {
        dx += std::cos(player_angle_ + 1.5707963f) * speed;
        dy += std::sin(player_angle_ + 1.5707963f) * speed;
    }
    if (input.move_east) {
        dx -= std::cos(player_angle_ + 1.5707963f) * speed;
        dy -= std::sin(player_angle_ + 1.5707963f) * speed;
    }

    if (dx != 0.0f || dy != 0.0f) {
        const std::int16_t before_x = player_x_;
        const std::int16_t before_y = player_y_;
        map.try_move(player_x_, player_y_, static_cast<int>(dx), static_cast<int>(dy), lines_);
        if (player_x_ != before_x || player_y_ != before_y) {
            changed = true;
        }
    }

    if (input.use) {
        try_use_nearby_thing(map);
        changed = true;
    }

    if (changed) {
        needs_redraw = true;
    }
}

void Game::run_tic(const GameInput& input, const Map& map) {
    ++gametic;

    if (view == View::Map) {
        run_map_tic(input, map);
    } else if (view == View::World) {
        run_world_tic(input, map);
    }

    if (view == View::Sprite) {
        const int frame = (gametic / 2) % kTrooFrameCount;
        if (frame != sprite_frame_) {
            sprite_frame_ = frame;
            needs_redraw = true;
        }
    }
}

void Game::draw(Screen& screen, const Wad& wad, const Map& map) const {
    screen.clear(0);

    switch (view) {
        case View::Title: {
            const auto lump = wad.find_lump("TITLEPIC");
            if (!lump || !draw_patch(screen, wad.lump_data(*lump), 0, 0)) {
                std::fprintf(stderr, "Failed to draw TITLEPIC\n");
            }
            break;
        }
        case View::Map:
            map.draw(screen, 176, 28, 112, 215, 231, player_x_, player_y_, things_, lines_);
            break;
        case View::Sprite: {
            const auto lump = wad.find_lump(kTrooFrames[sprite_frame_]);
            if (!lump ||
                !draw_patch(screen, wad.lump_data(*lump), Screen::kWidth / 2, Screen::kHeight / 2)) {
                std::fprintf(stderr, "Failed to draw %s\n", kTrooFrames[sprite_frame_]);
            }
            break;
        }
        case View::World:
            render3d_.render(screen, wad, map, lines_, static_cast<float>(player_x_),
                             static_cast<float>(player_y_), player_angle_);
            break;
    }
}
