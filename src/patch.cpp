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

}  // namespace

bool draw_patch(Screen& screen, const WadLumpData& lump, int x, int y) {
    if (lump.size < 8) {
        return false;
    }

    const std::uint8_t* data = lump.data;
    const std::int16_t width = read_i16(data);
    const std::int16_t height = read_i16(data + 2);
    const std::int16_t leftoffset = read_i16(data + 4);
    const std::int16_t topoffset = read_i16(data + 6);

    if (width <= 0 || height <= 0) {
        return false;
    }

    const std::size_t directory_bytes = 8u + static_cast<std::size_t>(width) * 4u;
    if (directory_bytes > lump.size) {
        return false;
    }

    const int dest_x = x - leftoffset;
    const int dest_y = y - topoffset;

    const std::int32_t first_column_ofs = read_i32(data + 8);
    const std::size_t raw_bytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (first_column_ofs < 8 && lump.size == 8u + raw_bytes) {
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

    for (int col = 0; col < width; ++col) {
        const std::int32_t column_ofs = read_i32(data + 8 + col * 4);
        if (column_ofs < 8 ||
            static_cast<std::size_t>(column_ofs) >= lump.size) {
            return false;
        }

        const std::uint8_t* source = data + column_ofs;
        const int pixel_x = dest_x + col;

        while (true) {
            if (static_cast<std::size_t>(source - data) >= lump.size) {
                return false;
            }

            const std::uint8_t top_delta = *source++;
            if (top_delta == 255) {
                break;
            }

            if (static_cast<std::size_t>(source - data) >= lump.size) {
                return false;
            }

            const std::uint8_t length = *source++;
            for (int row = 0; row < length; ++row) {
                if (static_cast<std::size_t>(source - data) >= lump.size) {
                    return false;
                }

                const std::uint8_t pixel = *source++;
                if (pixel == 0) {
                    continue;
                }

                screen.put_pixel(pixel_x, dest_y + top_delta + row, pixel);
            }
        }
    }

    return true;
}
