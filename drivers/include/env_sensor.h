/**
 * drivers/include/env_sensor.h
 * * Architecture Layer: Driver interface / Sensor abstraction
 * Responsibility: Abstract environmental sensor (temperature + pressure). Implemented by
 *                 MockEnvSensor today and by the BME280 driver on the STM32 (milestone M7).
 */
#pragma once
#include <cstdint>

#include "error_codes.h"

// Measurement concept
struct SensorReading {
    uint32_t timestampMs;
    float temperatureC;
    float pressureHpa;
};

// Sensor interface
class EnvSensor {
  public:
    EnvSensor() = default;
    EnvSensor(const EnvSensor&) = delete;
    EnvSensor& operator=(const EnvSensor&) = delete;
    virtual ~EnvSensor() = default;

    virtual ErrorCode init() = 0;
    // Fills temperatureC and pressureHpa. Returns SENSOR_ERROR if the device did not answer;
    // the caller decides whether to retry.
    virtual ErrorCode read(SensorReading& out) = 0;
};
