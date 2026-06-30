#include "log.hpp"

#include <array>
#include <cstdarg>
#include <cstdio>
#include <mutex>

namespace {

constexpr int kChannelCount = 7;
std::array<bool, kChannelCount> g_channel_enabled = {true, true, true, true, true, true, true};
std::mutex g_log_mutex;

int channel_index(Log::Channel channel) {
    return static_cast<int>(channel);
}

void write_v(Log::Channel channel, const char* level, const char* format, std::va_list args) {
    if (!Log::channel_enabled(channel)) {
        return;
    }

    std::lock_guard<std::mutex> lock(g_log_mutex);
    std::FILE* stream = std::strcmp(level, "ERROR") == 0 ? stderr : stdout;
    std::fprintf(stream, "[%s][%s] ", level, Log::channel_name(channel));
    std::vfprintf(stream, format, args);
    std::fprintf(stream, "\n");
    std::fflush(stream);
}

}  // namespace

namespace Log {

const char* channel_name(Channel channel) {
    switch (channel) {
        case Channel::General:
            return "general";
        case Channel::Wad:
            return "wad";
        case Channel::Asset:
            return "asset";
        case Channel::Renderer:
            return "renderer";
        case Channel::Debug:
            return "debug";
        case Channel::Map:
            return "map";
        case Channel::Game:
            return "game";
    }
    return "unknown";
}

void set_channel_enabled(Channel channel, bool enabled) {
    const int index = channel_index(channel);
    if (index < 0 || index >= kChannelCount) {
        return;
    }
    g_channel_enabled[static_cast<std::size_t>(index)] = enabled;
}

bool channel_enabled(Channel channel) {
    const int index = channel_index(channel);
    if (index < 0 || index >= kChannelCount) {
        return false;
    }
    return g_channel_enabled[static_cast<std::size_t>(index)];
}

void write(Channel channel, const char* level, const char* format, ...) {
    std::va_list args;
    va_start(args, format);
    write_v(channel, level, format, args);
    va_end(args);
}

void info(Channel channel, const char* format, ...) {
    std::va_list args;
    va_start(args, format);
    write_v(channel, "INFO", format, args);
    va_end(args);
}

void warn(Channel channel, const char* format, ...) {
    std::va_list args;
    va_start(args, format);
    write_v(channel, "WARN", format, args);
    va_end(args);
}

void error(Channel channel, const char* format, ...) {
    std::va_list args;
    va_start(args, format);
    write_v(channel, "ERROR", format, args);
    va_end(args);
}

}  // namespace Log
