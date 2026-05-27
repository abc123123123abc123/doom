#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct WadLump {
    int32_t offset = 0;
    int32_t size = 0;
    std::string name;
};

struct WadLumpData {
    const std::uint8_t* data = nullptr;
    std::size_t size = 0;
};

class Wad {
public:
    static std::optional<Wad> load(const std::string& path);

    const std::string& identification() const { return identification_; }
    int lump_count() const { return static_cast<int>(lumps_.size()); }
    const WadLump& lump(int index) const { return lumps_.at(index); }
    WadLumpData lump_data(int index) const;
    std::optional<int> find_lump(const std::string& name) const;

    void print_directory() const;

private:
    std::vector<std::uint8_t> data_;
    std::string identification_;
    std::vector<WadLump> lumps_;
};
