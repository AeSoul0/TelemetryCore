/**
 * app/src/acquisition.cpp
 * * Architecture Layer: Domain / Acquisition Implementation
 * Responsibility: Reads environment and battery with a bounded number of retries.
 */
#include "acquisition.h"

SensorSample Acquisition::acquire(uint32_t timestampMs) {
    SensorSample sample{};
    sample.reading.timestampMs = timestampMs;
    sample.error = ErrorCode::SENSOR_ERROR;

    for (uint8_t attempt = 1; attempt <= maxAttempts_; ++attempt) {
        sample.attempts = attempt;
        SensorReading reading{};
        float batteryPct = 0.0f;
        if (envSensor_.read(reading) == ErrorCode::OK && batteryGauge_.readPercent(batteryPct) == ErrorCode::OK) {
            sample.reading.temperatureC = reading.temperatureC;
            sample.reading.pressureHpa = reading.pressureHpa;
            sample.batteryPct = batteryPct;
            sample.error = ErrorCode::OK;
            break;
        }
    }
    return sample;
}
