/**
 * app/src/telemetry.cpp
 * * Architecture Layer: Domain / Processing Implementation
 * Responsibility: Validates samples, applies the fault policy and serialises telemetry packets
 *                 into a caller-provided buffer (no heap, no iostream, no float printf).
 */
#include "telemetry.h"

#include <cmath>
#include <cstdio>

namespace {

SystemStatus worst(SystemStatus a, SystemStatus b) {
    return (static_cast<uint8_t>(a) >= static_cast<uint8_t>(b)) ? a : b;
}

// Fixed-point representation with one decimal: avoids "%f", which newlib-nano does not support
// without -u _printf_float (~10 KB of flash).
struct Fixed1 {
    const char* sign;
    unsigned long whole;
    unsigned long tenths;
};

Fixed1 toFixed1(float value) {
    if (!std::isfinite(value)) {
        value = 0.0f;
    }
    const long scaled = std::lround(value * 10.0f);
    const auto magnitude = static_cast<unsigned long>(scaled < 0 ? -scaled : scaled);
    return Fixed1{scaled < 0 ? "-" : "", magnitude / 10UL, magnitude % 10UL};
}

unsigned long toPercent(float value) {
    if (!std::isfinite(value) || value <= 0.0f) {
        return 0UL;
    }
    if (value >= 100.0f) {
        return 100UL;
    }
    return static_cast<unsigned long>(std::lround(value));
}

} // namespace

const char* statusName(SystemStatus status) {
    switch (status) {
    case SystemStatus::OK:
        return "OK";
    case SystemStatus::WARNING:
        return "WARNING";
    case SystemStatus::CRITICAL:
        return "CRITICAL";
    }
    return "UNKNOWN";
}

SystemStatus TelemetryProcessor::calculateStatus(float temperatureC) const {
    if (!std::isfinite(temperatureC)) {
        return SystemStatus::CRITICAL;
    }
    if (temperatureC >= thresholds_.temperatureCriticalC) {
        return SystemStatus::CRITICAL;
    }
    if (temperatureC >= thresholds_.temperatureWarningC) {
        return SystemStatus::WARNING;
    }
    return SystemStatus::OK;
}

SystemStatus TelemetryProcessor::batteryStatus(float batteryPct) const {
    if (batteryPct < thresholds_.batteryCriticalPct) {
        return SystemStatus::CRITICAL;
    }
    if (batteryPct < thresholds_.batteryWarningPct) {
        return SystemStatus::WARNING;
    }
    return SystemStatus::OK;
}

bool TelemetryProcessor::isPlausible(const SensorSample& sample) const {
    const SensorReading& r = sample.reading;
    return std::isfinite(r.temperatureC) && std::isfinite(r.pressureHpa) && std::isfinite(sample.batteryPct) &&
           r.temperatureC >= thresholds_.temperatureMinC && r.temperatureC <= thresholds_.temperatureMaxC &&
           r.pressureHpa >= thresholds_.pressureMinHpa && r.pressureHpa <= thresholds_.pressureMaxHpa &&
           sample.batteryPct >= 0.0f && sample.batteryPct <= 100.0f;
}

TelemetryPacket TelemetryProcessor::process(const SensorSample& sample, uint32_t droppedSinceLast) {
    TelemetryPacket packet{};
    packet.sequence = sequence_++;
    packet.timestampMs = sample.reading.timestampMs;
    packet.faultFlags = (droppedSinceLast > 0) ? FaultFlags::kQueueOverflow : 0U;

    const bool acquired = (sample.error == ErrorCode::OK);
    const bool valid = acquired && isPlausible(sample);

    if (valid) {
        consecutiveFailures_ = 0;
        lastTemperatureC_ = sample.reading.temperatureC;
        lastPressureHpa_ = sample.reading.pressureHpa;
        lastBatteryPct_ = sample.batteryPct;
    } else {
        ++consecutiveFailures_;
        packet.faultFlags |= acquired ? FaultFlags::kInvalidData : FaultFlags::kSensorError;
    }

    packet.temperatureC = lastTemperatureC_;
    packet.pressureHpa = lastPressureHpa_;
    packet.batteryPct = lastBatteryPct_;

    if (valid) {
        packet.status = worst(calculateStatus(packet.temperatureC), batteryStatus(packet.batteryPct));
        if (packet.batteryPct < thresholds_.batteryWarningPct) {
            packet.faultFlags |= FaultFlags::kBatteryLow;
        }
    } else {
        packet.status =
            (consecutiveFailures_ >= thresholds_.failuresForCritical) ? SystemStatus::CRITICAL : SystemStatus::WARNING;
    }
    return packet;
}

size_t TelemetryProcessor::serialize(const TelemetryPacket& packet, char* buffer, size_t bufferSize) {
    if (buffer == nullptr || bufferSize == 0) {
        return 0;
    }
    const Fixed1 t = toFixed1(packet.temperatureC);
    const Fixed1 p = toFixed1(packet.pressureHpa);
    const int written = std::snprintf(
        buffer, bufferSize, "TLM,%lu,%lu,%s%lu.%lu,%s%lu.%lu,%lu,%s,%02X", static_cast<unsigned long>(packet.sequence),
        static_cast<unsigned long>(packet.timestampMs), t.sign, t.whole, t.tenths, p.sign, p.whole, p.tenths,
        toPercent(packet.batteryPct), statusName(packet.status), static_cast<unsigned int>(packet.faultFlags));
    if (written < 0 || static_cast<size_t>(written) >= bufferSize) {
        buffer[0] = '\0';
        return 0;
    }
    return static_cast<size_t>(written);
}
