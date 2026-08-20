#pragma once
#include <cstdint>

// FASE 3.1: Concetto di misura
struct SensorReading {
    uint32_t timestamp;
    float temperature;
};

// FASE 3.2: Interfaccia del sensore
class TemperatureSensor {
public:
    virtual ~TemperatureSensor() = default;
    virtual float readTemperature() = 0;
};

// FASE 3.3: Mock Sensor
class MockTemperatureSensor : public TemperatureSensor {
private:
    float baseTemp = 23.4f;
    int tick = 0;
public:
    float readTemperature() override;
};
