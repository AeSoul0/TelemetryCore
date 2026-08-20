#include "sensor.h"

// FASE 3.3 e 3.4: Implementazione Mock con anomalie
float MockTemperatureSensor::readTemperature() {
    tick++;
    
    // Inseriamo alcune anomalie
    if (tick % 7 == 0) return 82.5f; // Errore critico
    if (tick % 4 == 0) return 55.0f; // Errore warning
    
    // Normale andamento
    baseTemp += 0.2f;
    if (baseTemp > 25.0f) baseTemp = 23.4f;
    
    return baseTemp;
}
