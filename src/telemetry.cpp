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
    packet.pressure = 1013.25f; // Simulata (valore standard in hPa)
    packet.battery = 95.0f;     // Simulata (percentuale)
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
