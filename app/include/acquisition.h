/**
 * app/include/acquisition.h
 * * Architecture Layer: Domain / Acquisition
 * Responsibility: Obtains raw measurements from the sensors, retrying transient failures.
 *                 No interpretation happens here: validation belongs to the processor.
 */
#pragma once
#include <cstdint>

#include "battery_gauge.h"
#include "env_sensor.h"
#include "error_codes.h"

// One acquisition cycle, as sent from SensorTask to ProcessingTask.
struct SensorSample {
    SensorReading reading;
    float batteryPct;
    ErrorCode error;  // OK, or SENSOR_ERROR if every attempt failed
    uint8_t attempts; // reads performed (1 = first try succeeded)
};

class Acquisition {
  public:
    static constexpr uint8_t kDefaultMaxAttempts = 3;

    Acquisition(EnvSensor& envSensor, BatteryGauge& batteryGauge, uint8_t maxAttempts = kDefaultMaxAttempts)
        : envSensor_(envSensor), batteryGauge_(batteryGauge), maxAttempts_(maxAttempts == 0 ? 1 : maxAttempts) {}

    SensorSample acquire(uint32_t timestampMs);

  private:
    EnvSensor& envSensor_;
    BatteryGauge& batteryGauge_;
    uint8_t maxAttempts_;
};
