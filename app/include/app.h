/**
 * app/include/app.h
 * * Architecture Layer: Application / Tasks
 * Responsibility: Public entry points of the application. The board support code (bsp/) creates
 *                 the concrete drivers, passes them in, then starts the scheduler:
 *
 *     app::start(deps);          // creates queues, semaphores and tasks
 *     osal::startScheduler();    // FreeRTOS: never returns
 *
 * Nothing in app/ knows whether it runs on the PC or on the STM32.
 */
#pragma once
#include <cstdint>

#include "battery_gauge.h"
#include "env_sensor.h"
#include "error_codes.h"
#include "serial_port.h"

namespace app {

struct Dependencies {
    EnvSensor& envSensor;
    BatteryGauge& batteryGauge;
    SerialPort& serial;
    const char* boardName;
};

struct Stats {
    uint32_t samplesDropped; // SensorTask -> ProcessingTask queue overflows
    uint32_t packetsDropped; // ProcessingTask -> CommTask queue overflows
    uint32_t externalEvents; // events handled by EventTask
    uint32_t uplinkErrors;   // failed serial writes
};

const char* firmwareVersion();

// Must be called once, before osal::startScheduler(). Must not log or block.
ErrorCode start(const Dependencies& deps);

// Called by the board's interrupt handler (button, timer...). ISR-safe.
void onExternalEventFromIsr();

Stats stats();

} // namespace app
