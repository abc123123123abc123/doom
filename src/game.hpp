#pragma once

#include "map.hpp"
#include "render3d.hpp"
#include "screen.hpp"
#include "wad.hpp"

enum class View {
    Title,
    Map,
    Sprite,
    World,
};

struct GameInput {
    bool move_north = false;
    bool move_south = false;
    bool move_east = false;
    bool move_west = false;
    bool turn_left = false;
    bool turn_right = false;
    bool use = false;
};

class Game {
public:
    static constexpr int kTicRate = 35;
    static constexpr double kSecondsPerTic = 1.0 / static_cast<double>(kTicRate);
    static constexpr int kPlayerSpeed = 12;
    static constexpr int kUseDistance = 48;
    static constexpr int kDoorLinkDistance = 320;
    static constexpr float kTurnSpeed = 0.065f;

    int gametic = 0;
    View view = View::Title;
    bool needs_redraw = true;
    bool quit_requested = false;

    void init_from_map(const Map& map);
    void set_view(View view);
    void cycle_debug_value(SDL_Keycode key);
    void run_tic(const GameInput& input, const Map& map);
    void draw(Screen& screen, const Wad& wad, const Map& map, const Palette& palette) const;

    std::int16_t player_x() const { return player_x_; }
    std::int16_t player_y() const { return player_y_; }
    float player_angle() const { return player_angle_; }

private:
    void run_map_tic(const GameInput& input, const Map& map);
    void run_world_tic(const GameInput& input, const Map& map);
    void try_use_nearby_thing(const Map& map);
    void toggle_doors_near(const Map& map, std::int16_t x, std::int16_t y);
    void open_all_doors();

    Render3D render3d_;
    int wall_cycle_ = 0;
    int ceiling_cycle_ = 0;
    int floor_cycle_ = 0;
    int light_cycle_ = 5;
    int sprite_frame_ = 0;
    std::int16_t player_x_ = 0;
    std::int16_t player_y_ = 0;
    float player_angle_ = 0.0f;
    std::vector<MapThingState> things_;
    std::vector<MapLineState> lines_;
};
