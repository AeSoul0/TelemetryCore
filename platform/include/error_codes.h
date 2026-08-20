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
    SYSTEM_WARNING,
    SYSTEM_CRITICAL,
    SYSTEM_SHUTDOWN
};
