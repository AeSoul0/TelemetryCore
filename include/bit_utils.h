#pragma once
#include <cstdint>

// FASE 2: Manipolazione dei bit, fondamentale in embedded
namespace BitUtils {
    inline void setBit(uint8_t& reg, uint8_t bit) { reg |= (1 << bit); }
    inline void clearBit(uint8_t& reg, uint8_t bit) { reg &= ~(1 << bit); }
    inline void toggleBit(uint8_t& reg, uint8_t bit) { reg ^= (1 << bit); }
    inline bool readBit(uint8_t reg, uint8_t bit) { return (reg & (1 << bit)) != 0; }
}
