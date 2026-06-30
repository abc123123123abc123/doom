#pragma once

namespace Log {

enum class Channel {
    General,
    Wad,
    Asset,
    Renderer,
    Debug,
    Map,
    Game,
};

const char* channel_name(Channel channel);

void set_channel_enabled(Channel channel, bool enabled);
bool channel_enabled(Channel channel);

void write(Channel channel, const char* level, const char* format, ...);
void info(Channel channel, const char* format, ...);
void warn(Channel channel, const char* format, ...);
void error(Channel channel, const char* format, ...);

}  // namespace Log
