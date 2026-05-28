#pragma once

#include "screen.hpp"
#include "wad.hpp"

#include <cstdint>
#include <vector>

struct PatchInfo {
    int width = 0;
    int height = 0;
    int leftoffset = 0;
    int topoffset = 0;
    bool compact = false;
};

bool draw_patch(Screen& screen, const WadLumpData& lump, int x, int y);
bool draw_interface_raw_transparent(Screen& screen, const WadLumpData& lump, int x, int y,
                                    std::uint8_t transparent_index = 255);
bool draw_interface_raw_transparent_scaled(Screen& screen, const WadLumpData& lump, int x, int y,
                                           int dst_w, int dst_h,
                                           std::uint8_t transparent_index = 255);
bool patch_info(const WadLumpData& lump, PatchInfo& info);
bool patch_column_pixels(const WadLumpData& lump, int column, std::vector<std::uint8_t>& pixels);
bool patch_column_pixels_opaque(const WadLumpData& lump, int column,
                                std::vector<std::uint8_t>& pixels);
