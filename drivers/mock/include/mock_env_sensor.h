/**
 * drivers/mock/include/mock_env_sensor.h
 * * Architecture Layer: Driver / Mock implementation
 * Responsibility: Simulated temperature + pressure sensor with periodic anomalies and fault
 *                 injection. Portable: builds for the PC and for the STM32 (milestone M5).
 */
#pragma once
#include <atomic>
#include <cstdint>

#include "env_sensor.h"

class MockEnvSensor : public EnvSensor {
  public:
    struct Config {
        bool anomalies = true;       // inject WARNING / CRITICAL temperatures
        uint32_t warningEvery = 20;  // every N-th read returns kWarningTempC
        uint32_t criticalEvery = 50; // every N-th read returns kCriticalTempC
    };

    static constexpr float kWarningTempC = 55.0f;
    static constexpr float kCriticalTempC = 82.5f;

    MockEnvSensor() = default;
    explicit MockEnvSensor(const Config& config) : config_(config) {}

    ErrorCode init() override;
    ErrorCode read(SensorReading& out) override;

    // --- Fault injection (thread-safe: may be called from another task) -------------------------
    // The next `count` reads fail with SENSOR_ERROR.
    void injectFailures(uint32_t count) { pendingFailures_.fetch_add(count); }
    // While disconnected every read fails.
    void setDisconnected(bool disconnected) { disconnected_.store(disconnected); }
    // The next read succeeds but returns NaN (corrupted data).
    void injectInvalidValue() { pendingInvalid_.store(true); }

    uint32_t readCount() const { return tick_; }

  private:
    Config config_{};
    uint32_t tick_ = 0;
    float baseTempC_ = 23.4f;
    std::atomic<uint32_t> pendingFailures_{0};
    std::atomic<bool> disconnected_{false};
    std::atomic<bool> pendingInvalid_{false};
};
