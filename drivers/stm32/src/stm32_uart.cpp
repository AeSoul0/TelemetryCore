/**
 * drivers/stm32/src/stm32_uart.cpp
 * * Architecture Layer: Driver / STM32 implementation
 * Responsibility: Blocking UART transmission through the HAL.
 */
#include "stm32_uart.h"

ErrorCode Stm32Uart::init() {
    return (handle_.gState != HAL_UART_STATE_RESET) ? ErrorCode::OK : ErrorCode::NOT_INITIALIZED;
}

ErrorCode Stm32Uart::write(const uint8_t* data, size_t length, uint32_t timeoutMs) {
    if (length > UINT16_MAX) {
        return ErrorCode::INVALID_DATA;
    }
    const HAL_StatusTypeDef status = HAL_UART_Transmit(&handle_, data, static_cast<uint16_t>(length), timeoutMs);
    switch (status) {
    case HAL_OK:
        return ErrorCode::OK;
    case HAL_TIMEOUT:
        return ErrorCode::TIMEOUT;
    default:
        return ErrorCode::COMMUNICATION_ERROR;
    }
}
