/**
 * platform/include/bit_utils.h
 * * Architecture Layer: Utility / Hardware Abstraction
 * Responsibility: Provides fundamental bit manipulation helpers for embedded systems.
 *
 * Works on any unsigned integer width (8/16/32-bit registers). The mask is always built in the
 * register's own unsigned type, so `setBit(reg32, 31)` never shifts into the sign bit of an int.
 */
#pragma once
#include <cstdint>
#include <type_traits>

// Bit manipulation
namespace BitUtils {

template <typename T> constexpr T bitMask(uint8_t bit) {
    static_assert(std::is_unsigned_v<T>, "BitUtils works on unsigned register types only");
    return static_cast<T>(T{1} << bit);
}

template <typename T> constexpr void setBit(T& reg, uint8_t bit) {
    reg = static_cast<T>(reg | bitMask<T>(bit));
}

template <typename T> constexpr void clearBit(T& reg, uint8_t bit) {
    reg = static_cast<T>(reg & static_cast<T>(~bitMask<T>(bit)));
}

template <typename T> constexpr void toggleBit(T& reg, uint8_t bit) {
    reg = static_cast<T>(reg ^ bitMask<T>(bit));
}

template <typename T> constexpr bool readBit(T reg, uint8_t bit) {
    return (reg & bitMask<T>(bit)) != 0;
}

} // namespace BitUtils
