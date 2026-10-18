/**
 * platform/src/logger.cpp
 * * Architecture Layer: Platform / Logging
 * Responsibility: Formats log payloads and forwards them to the installed sink.
 */
#include "logger.h"

#include <atomic>
#include <cstdarg>
#include <cstdio>

namespace {

std::atomic<logger::Sink> gSink{nullptr};
std::atomic<logger::Level> gMinLevel{logger::Level::Debug};

} // namespace

namespace logger {

void setSink(Sink sink) {
    gSink.store(sink);
}
void setMinLevel(Level level) {
    gMinLevel.store(level);
}
Level minLevel() {
    return gMinLevel.load();
}

const char* levelName(Level level) {
    switch (level) {
    case Level::Debug:
        return "DEBUG";
    case Level::Info:
        return "INFO";
    case Level::Warn:
        return "WARN";
    case Level::Error:
        return "ERROR";
    case Level::Off:
        return "OFF";
    }
    return "?";
}

void log(Level level, const char* component, const char* format, ...) {
    const Sink sink = gSink.load();
    if (sink == nullptr || level == Level::Off ||
        static_cast<uint8_t>(level) < static_cast<uint8_t>(gMinLevel.load())) {
        return;
    }

    char payload[kMaxPayloadLength];
    const int header = std::snprintf(payload, sizeof(payload), "LOG,%s,%s,", levelName(level), component);
    if (header < 0) {
        return;
    }
    if (static_cast<size_t>(header) < sizeof(payload)) {
        va_list args; // NOLINT(cppcoreguidelines-init-variables): initialised by va_start
        va_start(args, format);
        // Output longer than the buffer is truncated (vsnprintf always NUL-terminates).
        (void)std::vsnprintf(payload + header, sizeof(payload) - static_cast<size_t>(header), format, args);
        va_end(args);
    }
    sink(payload);
}

} // namespace logger
