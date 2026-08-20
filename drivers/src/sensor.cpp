/**
 * src/sensor.cpp
 * * Architecture Layer: Domain / Sensor Implementation
 * Responsibility: Implements the mock sensor logic, generating normal temperature readings and periodic anomalies.
 */
#include "sensor.h"

// and 3.4: Mock implementation with anomalies
float MockTemperatureSensor::readTemperature() {
    tick++;
    
    // Inject some anomalies
    if (tick % 7 == 0) return 82.5f; // Critical error
    if (tick % 4 == 0) return 55.0f; // Warning error
    
    // Normal trend
    baseTemp += 0.2f;
    if (baseTemp > 25.0f) baseTemp = 23.4f;
    
    return baseTemp;
}
