#include "patch.hpp"

#include <cstdint>
#include <cstring>

namespace {

std::int16_t read_i16(const std::uint8_t* bytes) {
    std::int16_t value;
    std::memcpy(&value, bytes, sizeof(value));
    return value;
}

std::int32_t read_i32(const std::uint8_t* bytes) {
    std::int32_t value;
    std::memcpy(&value, bytes, sizeof(value));
    return value;
}

std::uint16_t read_u16(const std::uint8_t* bytes) {
    std::uint16_t value;
    std::memcpy(&value, bytes, sizeof(value));
    return value;
}

bool draw_columns(Screen& screen, const std::uint8_t* data, std::size_t lump_size, int width,
                  int height, int leftoffset, int topoffset, int x, int y,
                  bool column_offsets_are_32bit, bool pad_odd_posts) {
    const int dest_x = x - leftoffset;
    const int dest_y = y - topoffset;

    for (int col = 0; col < width; ++col) {
        std::size_t column_ofs = 0;
        if (column_offsets_are_32bit) {
            column_ofs = static_cast<std::size_t>(read_i32(data + 8 + col * 4));
        } else {
            column_ofs = read_u16(data + 8 + col * 2);
        }

        if (column_ofs < 8 || column_ofs >= lump_size) {
            return false;
        }

        const std::uint8_t* source = data + column_ofs;
        const int pixel_x = dest_x + col;

        while (true) {
            if (static_cast<std::size_t>(source - data) >= lump_size) {
                return false;
            }

            const std::uint8_t top_delta = *source++;
            if (top_delta == 255) {
                break;
            }

            if (static_cast<std::size_t>(source - data) >= lump_size) {
                return false;
            }

            const std::uint8_t length = *source++;
            for (int row = 0; row < length; ++row) {
                if (static_cast<std::size_t>(source - data) >= lump_size) {
                    return false;
                }

                const std::uint8_t pixel = *source++;
                if (pixel == 0) {
                    continue;
                }

                screen.put_pixel(pixel_x, dest_y + top_delta + row, pixel);
            }

            if (pad_odd_posts && (length & 1)) {
                if (static_cast<std::size_t>(source - data) >= lump_size) {
                    return false;
                }
                ++source;
            }
        }
    }

    return true;
}

bool compact_offsets_valid(const std::uint8_t* data, std::size_t lump_size, int width) {
    if (width <= 0) {
        return false;
    }

    const std::size_t directory_end = 8u + static_cast<std::size_t>(width) * 2u;
    if (directory_end > lump_size) {
        return false;
    }

    std::uint16_t previous = 0;
    for (int col = 0; col < width; ++col) {
        const std::uint16_t column_ofs = read_u16(data + 8 + col * 2);
        if (column_ofs < directory_end || static_cast<std::size_t>(column_ofs) >= lump_size) {
            return false;
        }
        if (col > 0 && column_ofs < previous) {
            return false;
        }
        previous = column_ofs;
    }
    return true;
}

int compact_sprite_width(const std::uint8_t* data, std::size_t lump_size) {
    int width = data[0];
    while (width > 0) {
        if (compact_offsets_valid(data, lump_size, width)) {
            return width;
        }
        --width;
    }
    return 0;
}

bool draw_raw_screen(Screen& screen, const std::uint8_t* data, std::size_t lump_size,
                     int width, int height, int leftoffset, int topoffset, int x, int y) {
    const std::size_t raw_bytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (lump_size != 8u + raw_bytes) {
        return false;
    }

    const int dest_x = x - leftoffset;
    const int dest_y = y - topoffset;
    const std::uint8_t* pixels = data + 8;

    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            const std::uint8_t pixel =
                pixels[static_cast<std::size_t>(row) * static_cast<std::size_t>(width) +
                       static_cast<std::size_t>(col)];
            screen.put_pixel(dest_x + col, dest_y + row, pixel);
        }
    }
    return true;
}

}  // namespace

bool draw_patch(Screen& screen, const WadLumpData& lump, int x, int y) {
    if (lump.size < 8) {
        return false;
    }

    const std::uint8_t* data = lump.data;

    const std::int16_t width16 = read_i16(data);
    const std::int16_t height16 = read_i16(data + 2);
    const std::int16_t leftoffset16 = read_i16(data + 4);
    const std::int16_t topoffset16 = read_i16(data + 6);

    if (width16 > 0 && width16 < 512 && height16 > 0 && height16 < 512) {
        const std::size_t directory_bytes = 8u + static_cast<std::size_t>(width16) * 4u;
        const std::int32_t first_column_ofs = read_i32(data + 8);
        if (directory_bytes <= lump.size && first_column_ofs >= 8 &&
            static_cast<std::size_t>(first_column_ofs) < lump.size) {
            return draw_columns(screen, data, lump.size, width16, height16, leftoffset16,
                                topoffset16, x, y, true, true);
        }

        if (first_column_ofs < 8) {
            return draw_raw_screen(screen, data, lump.size, width16, height16, leftoffset16,
                                   topoffset16, x, y);
        }
    }

    const int compact_width = compact_sprite_width(data, lump.size);
    const int compact_height = data[1];
    if (compact_width > 0 && compact_height > 0) {
        const int leftoffset = static_cast<int>(read_u16(data + 4));
        const int topoffset = static_cast<int>(read_u16(data + 6));
        return draw_columns(screen, data, lump.size, compact_width, compact_height, leftoffset,
                            topoffset, x, y, false, false);
    }

    return false;
}
