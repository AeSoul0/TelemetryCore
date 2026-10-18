/**
 * platform/include/error_codes.h
 * * Architecture Layer: Platform / Error Handling
 * Responsibility: Defines standard system-wide error codes.
 */
#pragma once
#include <cstdint>

// Error handling
enum class ErrorCode : uint8_t {
    OK = 0,
    SENSOR_ERROR,
    COMMUNICATION_ERROR,
    INVALID_DATA,
    QUEUE_FULL,
    TIMEOUT,
    NOT_INITIALIZED,
    BUFFER_TOO_SMALL,
    SYSTEM_WARNING,
    SYSTEM_CRITICAL,
    SYSTEM_SHUTDOWN
};

inline const char* errorCodeName(ErrorCode code) {
    switch (code) {
    case ErrorCode::OK:
        return "OK";
    case ErrorCode::SENSOR_ERROR:
        return "SENSOR_ERROR";
    case ErrorCode::COMMUNICATION_ERROR:
        return "COMMUNICATION_ERROR";
    case ErrorCode::INVALID_DATA:
        return "INVALID_DATA";
    case ErrorCode::QUEUE_FULL:
        return "QUEUE_FULL";
    case ErrorCode::TIMEOUT:
        return "TIMEOUT";
    case ErrorCode::NOT_INITIALIZED:
        return "NOT_INITIALIZED";
    case ErrorCode::BUFFER_TOO_SMALL:
        return "BUFFER_TOO_SMALL";
    case ErrorCode::SYSTEM_WARNING:
        return "SYSTEM_WARNING";
    case ErrorCode::SYSTEM_CRITICAL:
        return "SYSTEM_CRITICAL";
    case ErrorCode::SYSTEM_SHUTDOWN:
        return "SYSTEM_SHUTDOWN";
    }
    return "UNKNOWN";
}
