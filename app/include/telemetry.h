/**
 * include/telemetry.h
 * * Architecture Layer: Domain / Processing
 * Responsibility: Defines the telemetry data model and the processor interface that evaluates sensor readings.
 */
#pragma once
#include <cstdint>
#include <string>
#include "sensor.h"

// Phase 4.2: System status
enum class SystemStatus : uint8_t {
    OK,
    WARNING,
    CRITICAL
};

// Phase 4.1: Telemetry packet
struct TelemetryPacket {
    uint32_t timestamp;
    float temperature;
    float pressure;
    float battery;
    SystemStatus status;
};

// Phase 5.2 & 5.3: Processing and Telemetry
class TelemetryProcessor {
public:
    // Phase 4.3: Rules
    SystemStatus calculateStatus(float temperature) const;
    
    // Acquires from reading and produces packet
    TelemetryPacket process(const SensorReading& reading) const;
    
    // Phase 4.4: Serialization
    std::string serialize(const TelemetryPacket& packet) const;
};
