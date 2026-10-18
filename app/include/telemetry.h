/**
 * app/include/telemetry.h
 * * Architecture Layer: Domain / Processing
 * Responsibility: Telemetry data model, validation rules, fault policy and serialisation.
 *
 * Fault policy (roadmap phase 16):
 *   - a failed read is retried by the acquisition layer (see acquisition.h);
 *   - a sample still invalid after the retries is a persistent error  -> WARNING;
 *   - kFailuresForCritical consecutive invalid samples are a severe error -> CRITICAL;
 *   - the first valid sample restores the threshold-based status.
 */
#pragma once
#include <cstddef>
#include <cstdint>

#include "acquisition.h"

// System status
enum class SystemStatus : uint8_t { OK, WARNING, CRITICAL };

const char* statusName(SystemStatus status);

// Bits of TelemetryPacket::faultFlags
namespace FaultFlags {
constexpr uint8_t kSensorError = 1U << 0;   // sample missing after all retries
constexpr uint8_t kInvalidData = 1U << 1;   // sample received but implausible (NaN, out of range)
constexpr uint8_t kQueueOverflow = 1U << 2; // samples or packets dropped since the previous packet
constexpr uint8_t kBatteryLow = 1U << 3;    // battery below the warning threshold
} // namespace FaultFlags

// Telemetry packet
struct TelemetryPacket {
    uint32_t sequence;
    uint32_t timestampMs;
    float temperatureC;
    float pressureHpa;
    float batteryPct;
    SystemStatus status;
    uint8_t faultFlags;
};

struct Thresholds {
    float temperatureWarningC = 50.0f;
    float temperatureCriticalC = 80.0f;
    float batteryWarningPct = 20.0f;
    float batteryCriticalPct = 5.0f;
    // Plausibility limits (BME280 operating range is -40..85 °C, 300..1100 hPa)
    float temperatureMinC = -40.0f;
    float temperatureMaxC = 125.0f;
    float pressureMinHpa = 300.0f;
    float pressureMaxHpa = 1100.0f;
    uint32_t failuresForCritical = 10; // 10 samples = 1 s at 100 ms
};

// Processing and Telemetry
class TelemetryProcessor {
  public:
    TelemetryProcessor() = default;
    explicit TelemetryProcessor(const Thresholds& thresholds) : thresholds_(thresholds) {}

    // Rules
    SystemStatus calculateStatus(float temperatureC) const;
    SystemStatus batteryStatus(float batteryPct) const;
    bool isPlausible(const SensorSample& sample) const;

    // Acquires from a sample and produces a packet. `droppedSinceLast` is the number of samples or
    // packets lost in the pipeline since the previous call (raises kQueueOverflow).
    TelemetryPacket process(const SensorSample& sample, uint32_t droppedSinceLast = 0);

    uint32_t consecutiveFailures() const { return consecutiveFailures_; }

    // Serialization: writes the payload "TLM,<seq>,<ms>,<T>,<P>,<B>,<STATUS>,<FLAGS>" (no framing).
    // Returns the payload length, or 0 if the buffer is too small.
    static size_t serialize(const TelemetryPacket& packet, char* buffer, size_t bufferSize);

  private:
    Thresholds thresholds_{};
    uint32_t sequence_ = 0;
    uint32_t consecutiveFailures_ = 0;
    // Last plausible values, reported (with a fault flag) while the sensor is failing.
    float lastTemperatureC_ = 0.0f;
    float lastPressureHpa_ = 0.0f;
    float lastBatteryPct_ = 0.0f;
};
