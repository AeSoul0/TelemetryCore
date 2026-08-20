/**
 * src/main.cpp
 * * Architecture Layer: Application / Main
 * Responsibility: Entry point for the application. Coordinates sensor acquisition and telemetry processing.
 */
#include <iostream>
#include <thread>
#include <chrono>
#include "bit_utils.h"
#include "sensor.h"
#include "telemetry.h"

int main() {
    std::cout << "=== PHASE 2: Memory & Bit Manipulation Test ===\n";
    uint8_t flags = 0;
    BitUtils::setBit(flags, 3);
    std::cout << "Flags after setBit(3): " << static_cast<int>(flags) << "\n";
    BitUtils::toggleBit(flags, 3);
    std::cout << "Flags after toggleBit(3): " << static_cast<int>(flags) << "\n";
    std::cout << "----------------------------------------------\n\n";

    std::cout << "=== PHASE 3, 4, 5, 6: Sensor & Telemetry ===\n";
    MockTemperatureSensor sensor;
    TelemetryProcessor processor;

    uint32_t current_time = 0;
    
    // Execute 10 cycles of acquisition and serialization
    for (int i = 1; i <= 10; ++i) {
        // Phase 5.1: Acquisition
        SensorReading reading;
        reading.timestamp = current_time;
        reading.temperature = sensor.readTemperature();

        // Phase 5.2: Processing
        TelemetryPacket packet = processor.process(reading);

        // Phase 5.3: Telemetry (Serialization)
        std::string serialized = processor.serialize(packet);

        // Output to PC (simulates CommunicationTask for now)
        std::cout << "[" << packet.timestamp << "] " << serialized << "\n";
        
        current_time += 100;
        // Simulate system delay to avoid unnecessary CPU consumption
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\nTest successfully completed.\n";
    return 0;
}
