/**
 * include/sensor.h
 * * Architecture Layer: Domain / Sensor Abstraction
 * Responsibility: Defines the abstract interface for temperature sensors and provides a simulated implementation for testing.
 */
#pragma once
#include <cstdint>

// Measurement concept
struct SensorReading {
    uint32_t timestamp;
    float temperature;
};

// Sensor interface
class TemperatureSensor {
public:
    virtual ~TemperatureSensor() = default;
    virtual float readTemperature() = 0;
};

// Mock Sensor
class MockTemperatureSensor : public TemperatureSensor {
private:
    float baseTemp = 23.4f;
    int tick = 0;
public:
    float readTemperature() override;
};
