/**
 * drivers/mock/src/mock_battery_gauge.cpp
 * * Architecture Layer: Driver / Mock implementation
 * Responsibility: Linear discharge model of the simulated battery.
 */
#include "mock_battery_gauge.h"

ErrorCode MockBatteryGauge::init() {
    levelPercent_ = config_.startPercent;
    return ErrorCode::OK;
}

ErrorCode MockBatteryGauge::readPercent(float& percent) {
    percent = levelPercent_;
    levelPercent_ -= config_.dischargePerRead;
    if (levelPercent_ < config_.replaceBelowPercent) {
        levelPercent_ = config_.startPercent;
    }
    return ErrorCode::OK;
}
