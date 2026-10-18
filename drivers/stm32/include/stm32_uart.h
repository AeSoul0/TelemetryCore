/**
 * drivers/stm32/include/stm32_uart.h
 * * Architecture Layer: Driver / STM32 implementation
 * Responsibility: SerialPort over an STM32 HAL UART. This first version transmits in blocking
 *                 (polling) mode: simple and good enough for ~10 frames/s at 115200 baud.
 *                 Milestone M6 adds interrupt-driven RX, milestone M10 moves TX to DMA.
 */
#pragma once
#include "serial_port.h"
#include "stm32f4xx_hal.h"

class Stm32Uart : public SerialPort {
  public:
    // The handle must already be initialised (HAL_UART_Init) by the board code.
    explicit Stm32Uart(UART_HandleTypeDef& handle) : handle_(handle) {}

    ErrorCode init() override;
    ErrorCode write(const uint8_t* data, size_t length, uint32_t timeoutMs) override;

  private:
    UART_HandleTypeDef& handle_;
};
