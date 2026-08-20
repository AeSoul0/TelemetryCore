/**
 * src/telemetry.cpp
 * * Architecture Layer: Domain / Processing Implementation
 * Responsibility: Implements the processing logic to evaluate sensor data and serialize telemetry packets.
 */
#include "telemetry.h"
#include <sstream>
#include <iomanip>

SystemStatus TelemetryProcessor::calculateStatus(float temperature) const {
    if (temperature >= 80.0f) return SystemStatus::CRITICAL;
    if (temperature >= 50.0f) return SystemStatus::WARNING;
    return SystemStatus::OK;
}

TelemetryPacket TelemetryProcessor::process(const SensorReading& reading) const {
    TelemetryPacket packet;
    packet.timestamp = reading.timestamp;
    packet.temperature = reading.temperature;
    packet.pressure = 1013.25f; // Simulated (standard value in hPa)
    packet.battery = 95.0f;     // Simulated (percentage)
    packet.status = calculateStatus(reading.temperature);
    return packet;
}

std::string TelemetryProcessor::serialize(const TelemetryPacket& packet) const {
    std::ostringstream oss;
    oss << "T=" << std::fixed << std::setprecision(1) << packet.temperature
        << ";P=" << packet.pressure
        << ";B=" << packet.battery
        << ";S=";
        
    switch(packet.status) {
        case SystemStatus::OK:       oss << "OK"; break;
        case SystemStatus::WARNING:  oss << "WARNING"; break;
        case SystemStatus::CRITICAL: oss << "CRITICAL"; break;
    }
    
    return oss.str();
}
