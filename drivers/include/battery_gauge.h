/**
 * drivers/include/battery_gauge.h
 * * Architecture Layer: Driver interface / Battery abstraction
 * Responsibility: Abstract battery level source. Implemented by MockBatteryGauge today and by an
 *                 ADC channel (potentiometer on PA0) on the STM32 (milestone M7).
 */
#pragma once
#include "error_codes.h"

class BatteryGauge {
  public:
    BatteryGauge() = default;
    BatteryGauge(const BatteryGauge&) = delete;
    BatteryGauge& operator=(const BatteryGauge&) = delete;
    virtual ~BatteryGauge() = default;

    virtual ErrorCode init() = 0;
    // Battery level in percent, 0..100.
    virtual ErrorCode readPercent(float& percent) = 0;
};
