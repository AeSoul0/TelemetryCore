/**
 * bsp/nucleo_f446re/src/main.cpp
 * * Architecture Layer: Board support / Firmware entry point
 * Responsibility: Brings up the NUCLEO-F446RE, wires the drivers into the application and starts
 *                 FreeRTOS. Same app/ code as the PC simulation; only this file and the drivers differ.
 *
 * Boot sequence:
 *   Reset_Handler (startup .s) -> SystemInit() -> .data/.bss init -> static constructors -> main()
 *   main(): HAL_Init -> 180 MHz clock -> GPIO/UART -> app::start() -> vTaskStartScheduler()
 *
 * Sensor and battery are still the mock drivers (milestone M5); the BME280 and ADC drivers
 * replace them in milestone M7 by changing only the two objects below.
 */
#include "app.h"
#include "board.h"
#include "mock_battery_gauge.h"
#include "mock_env_sensor.h"
#include "osal/osal.h"
#include "stm32_uart.h"

namespace {

constexpr uint32_t kHeartbeatPeriodMs = 500;

MockEnvSensor envSensor;
MockBatteryGauge batteryGauge;
Stm32Uart console(huart2);
osal::Task<128> heartbeatTask;

// LD2 blinks at 1 Hz while the scheduler is alive.
void heartbeatFn(void* /*arg*/) {
    uint32_t lastWakeMs = osal::nowMs();
    for (;;) {
        board_led_toggle();
        osal::delayUntil(lastWakeMs, kHeartbeatPeriodMs);
    }
}

} // namespace

int main() {
    if (HAL_Init() != HAL_OK || board_clock_init() != HAL_OK) {
        Error_Handler();
    }
    board_gpio_init();
    if (board_uart_init() != HAL_OK) {
        Error_Handler();
    }

    const app::Dependencies deps{envSensor, batteryGauge, console, BOARD_NAME};
    if (app::start(deps) != ErrorCode::OK) {
        Error_Handler();
    }
    if (heartbeatTask.start("Heartbeat", osal::Priority::Low, heartbeatFn) != ErrorCode::OK) {
        Error_Handler();
    }

    osal::startScheduler(); // never returns
    return 0;
}

extern "C" {

// B1 pressed: runs in interrupt context (EXTI15_10, NVIC priority 6).
void HAL_GPIO_EXTI_Callback(uint16_t pin) {
    if (pin == BUTTON_PIN) {
        app::onExternalEventFromIsr();
    }
}

// Called if a pure virtual function is ever invoked. Defining it here avoids pulling the
// libstdc++ terminate handler (and its demangler) into the image.
void __cxa_pure_virtual() {
    for (;;) {
    }
}

} // extern "C"
