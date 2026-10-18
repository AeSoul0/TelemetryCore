/**
 * tests/test_support.h
 * Responsibility: Test doubles shared by several test files.
 */
#pragma once
#include <mutex>
#include <string>
#include <vector>

#include "battery_gauge.h"
#include "env_sensor.h"
#include "serial_port.h"

// Sensor that returns scripted results, one per read() call; the last entry repeats.
class ScriptedEnvSensor : public EnvSensor {
  public:
    struct Step {
        ErrorCode result;
        float temperatureC;
        float pressureHpa;
    };

    explicit ScriptedEnvSensor(std::vector<Step> steps) : steps_(std::move(steps)) {}

    ErrorCode init() override { return ErrorCode::OK; }
    ErrorCode read(SensorReading& out) override {
        const Step& step = steps_[index_ < steps_.size() ? index_ : steps_.size() - 1];
        ++index_;
        ++reads;
        out.temperatureC = step.temperatureC;
        out.pressureHpa = step.pressureHpa;
        return step.result;
    }

    int reads = 0;

  private:
    std::vector<Step> steps_;
    size_t index_ = 0;
};

class FixedBatteryGauge : public BatteryGauge {
  public:
    explicit FixedBatteryGauge(float percent) : percent_(percent) {}
    ErrorCode init() override { return ErrorCode::OK; }
    ErrorCode readPercent(float& percent) override {
        percent = percent_;
        return ErrorCode::OK;
    }

  private:
    float percent_;
};

// Serial port that records everything written, split into lines.
class CaptureSerial : public SerialPort {
  public:
    ErrorCode init() override { return ErrorCode::OK; }
    ErrorCode write(const uint8_t* data, size_t length, uint32_t /*timeoutMs*/) override {
        std::lock_guard<std::mutex> lock(mutex_);
        buffer_.append(reinterpret_cast<const char*>(data), length);
        return ErrorCode::OK;
    }

    std::vector<std::string> lines() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> result;
        size_t start = 0;
        size_t end = 0;
        while ((end = buffer_.find("\r\n", start)) != std::string::npos) {
            result.push_back(buffer_.substr(start, end - start));
            start = end + 2;
        }
        return result;
    }

  private:
    mutable std::mutex mutex_;
    std::string buffer_;
};
