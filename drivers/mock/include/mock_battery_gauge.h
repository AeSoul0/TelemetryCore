/**
 * drivers/mock/include/mock_battery_gauge.h
 * * Architecture Layer: Driver / Mock implementation
 * Responsibility: Simulated battery that discharges linearly and is "replaced" when nearly empty,
 *                 so the battery thresholds (WARNING / CRITICAL) are exercised periodically.
 */
#pragma once
#include "battery_gauge.h"

class MockBatteryGauge : public BatteryGauge {
  public:
    struct Config {
        float startPercent = 100.0f;
        float dischargePerRead = 0.1f; // 10 reads/s -> 1 %/s
        float replaceBelowPercent = 3.0f;
    };

    MockBatteryGauge() = default;
    explicit MockBatteryGauge(const Config& config) : config_(config), levelPercent_(config.startPercent) {}

    ErrorCode init() override;
    ErrorCode readPercent(float& percent) override;

  private:
    Config config_{};
    float levelPercent_ = 100.0f;
};
