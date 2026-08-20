#pragma once
#include <cstdint>
#include <string>
#include "sensor.h"

// FASE 4.2: Stato del sistema
enum class SystemStatus : uint8_t {
    OK,
    WARNING,
    CRITICAL
};

// FASE 4.1: Packet di telemetria
struct TelemetryPacket {
    uint32_t timestamp;
    float temperature;
    float pressure;
    float battery;
    SystemStatus status;
};

// FASE 5.2 & 5.3: Processing and Telemetry
class TelemetryProcessor {
public:
    // FASE 4.3: Regole
    SystemStatus calculateStatus(float temperature) const;
    
    // Acquisisce da reading e produce packet
    TelemetryPacket process(const SensorReading& reading) const;
    
    // FASE 4.4: Serialization
    std::string serialize(const TelemetryPacket& packet) const;
};
