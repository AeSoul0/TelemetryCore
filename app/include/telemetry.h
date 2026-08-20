/**
 * include/telemetry.h
 * * Architecture Layer: Domain / Processing
 * Responsibility: Defines the telemetry data model and the processor interface that evaluates sensor readings.
 */
#pragma once
#include <cstdint>
#include <string>
#include "sensor.h"

// System status
enum class SystemStatus : uint8_t {
    OK,
    WARNING,
    CRITICAL
};

// Telemetry packet
struct TelemetryPacket {
    uint32_t timestamp;
    float temperature;
    float pressure;
    float battery;
    SystemStatus status;
};

// Processing and Telemetry
class TelemetryProcessor {
public:
    // Rules
    SystemStatus calculateStatus(float temperature) const;
    
    // Acquires from reading and produces packet
    TelemetryPacket process(const SensorReading& reading) const;
    
    // Serialization
    std::string serialize(const TelemetryPacket& packet) const;
};
