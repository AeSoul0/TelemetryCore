/**
 * include/bit_utils.h
 * * Architecture Layer: Utility / Hardware Abstraction
 * Responsibility: Provides fundamental bit manipulation macros and functions for embedded systems.
 */

#pragma once
#include <cstdint>

// Phase 2: Bit manipulation
namespace BitUtils {
    inline void setBit(uint8_t& reg, uint8_t bit) { reg |= (1 << bit); }
    inline void clearBit(uint8_t& reg, uint8_t bit) { reg &= ~(1 << bit); }
    inline void toggleBit(uint8_t& reg, uint8_t bit) { reg ^= (1 << bit); }
    inline bool readBit(uint8_t reg, uint8_t bit) { return (reg & (1 << bit)) != 0; }
}
