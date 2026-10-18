/**
 * platform/include/logger.h
 * * Architecture Layer: Platform / Logging
 * Responsibility: Structured, printf-style logging without heap or iostream.
 *
 * Each call formats one payload "LOG,<LEVEL>,<component>,<message>" into a buffer on the caller's
 * stack and hands it to the installed sink (the application frames it and writes it to the
 * serial link). The logger has no shared state besides the sink, so it is safe to call from any
 * task. It is NOT safe from an ISR: the sink usually takes a mutex.
 *
 * Verbosity:
 *   - compile time: TELEMETRY_LOG_LEVEL (0=DEBUG, 1=INFO, 2=WARN, 3=ERROR, 4=OFF). Filtered calls
 *     are removed by the compiler, but their arguments are still type-checked.
 *   - run time:     logger::setMinLevel() can raise the threshold further.
 */
#pragma once
#include <cstddef>
#include <cstdint>

#ifndef TELEMETRY_LOG_LEVEL
#define TELEMETRY_LOG_LEVEL 0
#endif

#if defined(__GNUC__)
#define TELEMETRY_PRINTF_FORMAT(fmtIndex, argsIndex) __attribute__((format(printf, fmtIndex, argsIndex)))
#else
#define TELEMETRY_PRINTF_FORMAT(fmtIndex, argsIndex)
#endif

namespace logger {

enum class Level : uint8_t { Debug = 0, Info = 1, Warn = 2, Error = 3, Off = 4 };

constexpr size_t kMaxPayloadLength = 128;

// Receives a NUL-terminated payload. Must be callable from any task.
using Sink = void (*)(const char* payload);

void setSink(Sink sink);
void setMinLevel(Level level);
Level minLevel();
const char* levelName(Level level);

void log(Level level, const char* component, const char* format, ...) TELEMETRY_PRINTF_FORMAT(3, 4);

constexpr int kCompiledLevel = TELEMETRY_LOG_LEVEL;
constexpr bool compiledIn(Level level) {
    return static_cast<int>(level) >= kCompiledLevel;
}

} // namespace logger

#define TELEMETRY_LOG(level, component, ...)                                                                           \
    do {                                                                                                               \
        if constexpr (::logger::compiledIn(level)) {                                                                   \
            ::logger::log(level, component, __VA_ARGS__);                                                              \
        }                                                                                                              \
    } while (false)

#define LOG_DEBUG(component, ...) TELEMETRY_LOG(::logger::Level::Debug, component, __VA_ARGS__)
#define LOG_INFO(component, ...) TELEMETRY_LOG(::logger::Level::Info, component, __VA_ARGS__)
#define LOG_WARN(component, ...) TELEMETRY_LOG(::logger::Level::Warn, component, __VA_ARGS__)
#define LOG_ERROR(component, ...) TELEMETRY_LOG(::logger::Level::Error, component, __VA_ARGS__)
