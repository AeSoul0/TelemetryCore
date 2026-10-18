/**
 * drivers/include/serial_port.h
 * * Architecture Layer: Driver interface / Serial communication
 * Responsibility: Abstract byte-oriented serial link. Implemented by StdoutSerial on the PC and by
 *                 Stm32Uart (USART2 -> ST-LINK virtual COM port) on the STM32.
 */
#pragma once
#include <cstddef>
#include <cstdint>

#include "error_codes.h"

class SerialPort {
  public:
    SerialPort() = default;
    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;
    virtual ~SerialPort() = default;

    virtual ErrorCode init() = 0;
    // Writes all bytes or fails. Implementations are not required to be thread-safe:
    // callers serialise access (see app::Uplink).
    virtual ErrorCode write(const uint8_t* data, size_t length, uint32_t timeoutMs) = 0;
};
