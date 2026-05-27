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
bool patch_info(const WadLumpData& lump, PatchInfo& info);
bool patch_column_pixels(const WadLumpData& lump, int column, std::vector<std::uint8_t>& pixels);
