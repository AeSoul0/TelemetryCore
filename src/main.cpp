#include <iostream>
#include <thread>
#include <chrono>
#include "bit_utils.h"
#include "sensor.h"
#include "telemetry.h"

int main() {
    std::cout << "=== FASE 2: Memory & Bit Manipulation Test ===\n";
    uint8_t flags = 0;
    BitUtils::setBit(flags, 3);
    std::cout << "Flags dopo setBit(3): " << static_cast<int>(flags) << "\n";
    BitUtils::toggleBit(flags, 3);
    std::cout << "Flags dopo toggleBit(3): " << static_cast<int>(flags) << "\n";
    std::cout << "----------------------------------------------\n\n";

    std::cout << "=== FASE 3, 4, 5, 6: Sensore & Telemetria ===\n";
    MockTemperatureSensor sensor;
    TelemetryProcessor processor;

    uint32_t current_time = 0;
    
    // Eseguiamo 10 cicli di acquisizione e serializzazione
    for (int i = 1; i <= 10; ++i) {
        // FASE 5.1: Acquisition
        SensorReading reading;
        reading.timestamp = current_time;
        reading.temperature = sensor.readTemperature();

        // FASE 5.2: Processing
        TelemetryPacket packet = processor.process(reading);

        // FASE 5.3: Telemetry (Serialization)
        std::string serialized = processor.serialize(packet);

        // Output al PC (simula CommunicationTask per ora)
        std::cout << "[" << packet.timestamp << "] " << serialized << "\n";
        
        current_time += 100;
        // Simula delay del sistema per non consumare CPU inutilmente
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\nTest concluso con successo.\n";
    return 0;
}
