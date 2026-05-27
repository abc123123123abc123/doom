#include "wad.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>

namespace {

std::int32_t read_i32(const std::uint8_t* bytes) {
    std::int32_t value;
    std::memcpy(&value, bytes, sizeof(value));
    return value;
}

std::string read_lump_name(const char* bytes) {
    std::string name(bytes, 8);
    while (!name.empty() && name.back() == ' ') {
        name.pop_back();
    }
    return name;
}

}  // namespace

std::optional<Wad> Wad::load(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return std::nullopt;
    }

    file.seekg(0, std::ios::end);
    const auto file_size = file.tellg();
    if (file_size < 12) {
        return std::nullopt;
    }

    file.seekg(0, std::ios::beg);
    Wad wad;
    wad.data_.resize(static_cast<std::size_t>(file_size));
    if (!file.read(reinterpret_cast<char*>(wad.data_.data()), file_size)) {
        return std::nullopt;
    }

    const auto* header = wad.data_.data();
    wad.identification_.assign(reinterpret_cast<const char*>(header), 4);
    if (wad.identification_ != "IWAD" && wad.identification_ != "PWAD") {
        return std::nullopt;
    }

    const std::int32_t num_lumps = read_i32(header + 4);
    const std::int32_t info_table_ofs = read_i32(header + 8);
    if (num_lumps < 0 || info_table_ofs < 0) {
        return std::nullopt;
    }

    const std::size_t directory_bytes = static_cast<std::size_t>(num_lumps) * 16u;
    if (static_cast<std::size_t>(info_table_ofs) + directory_bytes > wad.data_.size()) {
        return std::nullopt;
    }

    wad.lumps_.reserve(static_cast<std::size_t>(num_lumps));
    const auto* directory = wad.data_.data() + info_table_ofs;
    for (std::int32_t i = 0; i < num_lumps; ++i) {
        const auto* entry = directory + static_cast<std::size_t>(i) * 16u;
        WadLump lump;
        lump.offset = read_i32(entry);
        lump.size = read_i32(entry + 4);
        lump.name = read_lump_name(reinterpret_cast<const char*>(entry + 8));

        const std::size_t end =
            static_cast<std::size_t>(lump.offset) + static_cast<std::size_t>(lump.size);
        if (lump.offset < 0 || lump.size < 0 || end > wad.data_.size()) {
            return std::nullopt;
        }

        wad.lumps_.push_back(std::move(lump));
    }

    return wad;
}

WadLumpData Wad::lump_data(int index) const {
    const WadLump& lump = lumps_.at(index);
    return {data_.data() + lump.offset, static_cast<std::size_t>(lump.size)};
}

void Wad::print_directory() const {
    std::printf("WAD: %s (%d lumps)\n", identification_.c_str(), lump_count());
    std::printf("%4s  %-8s  %10s  %10s\n", "#", "name", "offset", "size");
    for (int i = 0; i < lump_count(); ++i) {
        const WadLump& lump = lumps_[static_cast<std::size_t>(i)];
        std::printf("%4d  %-8s  %10d  %10d\n", i, lump.name.c_str(), lump.offset, lump.size);
    }
}
