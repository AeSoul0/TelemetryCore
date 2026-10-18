/**
 * drivers/mock/src/mock_env_sensor.cpp
 * * Architecture Layer: Driver / Mock implementation
 * Responsibility: Generates a slow temperature trend, a pressure oscillation and periodic anomalies.
 */
#include "mock_env_sensor.h"

#include <limits>

namespace {

constexpr float kStandardPressureHpa = 1013.25f;

// Triangle wave in [-0.5, +0.5] hPa with a period of 40 reads: avoids pulling sinf() into firmware.
float pressureOffset(uint32_t tick) {
    const auto phase = static_cast<float>(tick % 40U);
    return (phase < 20.0f) ? (phase / 20.0f) - 0.5f : 0.5f - ((phase - 20.0f) / 20.0f);
}

} // namespace

ErrorCode MockEnvSensor::init() {
    tick_ = 0;
    baseTempC_ = 23.4f;
    return ErrorCode::OK;
}

ErrorCode MockEnvSensor::read(SensorReading& out) {
    ++tick_;

    if (disconnected_.load()) {
        return ErrorCode::SENSOR_ERROR;
    }
    uint32_t failures = pendingFailures_.load();
    while (failures > 0 && !pendingFailures_.compare_exchange_weak(failures, failures - 1)) {
    }
    if (failures > 0) {
        return ErrorCode::SENSOR_ERROR;
    }

    out.pressureHpa = kStandardPressureHpa + pressureOffset(tick_);

    if (pendingInvalid_.exchange(false)) {
        out.temperatureC = std::numeric_limits<float>::quiet_NaN();
        return ErrorCode::OK;
    }
    if (config_.anomalies && config_.criticalEvery > 0 && tick_ % config_.criticalEvery == 0) {
        out.temperatureC = kCriticalTempC;
        return ErrorCode::OK;
    }
    if (config_.anomalies && config_.warningEvery > 0 && tick_ % config_.warningEvery == 0) {
        out.temperatureC = kWarningTempC;
        return ErrorCode::OK;
    }

    // Normal trend: 23.4 -> 25.0 °C, then back
    baseTempC_ += 0.2f;
    if (baseTempC_ > 25.0f) {
        baseTempC_ = 23.4f;
    }
    out.temperatureC = baseTempC_;
    return ErrorCode::OK;
}
