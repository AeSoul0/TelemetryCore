/**
 * drivers/host/include/stdout_serial.h
 * * Architecture Layer: Driver / Host implementation
 * Responsibility: SerialPort that writes to the process stdout, so the simulator output can be
 *                 piped straight into the ground station.
 */
#pragma once
#include "serial_port.h"

class StdoutSerial : public SerialPort {
  public:
    ErrorCode init() override;
    ErrorCode write(const uint8_t* data, size_t length, uint32_t timeoutMs) override;
};
