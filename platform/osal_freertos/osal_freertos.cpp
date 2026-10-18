/**
 * platform/osal_freertos/osal_freertos.cpp
 * * Architecture Layer: Platform / OSAL FreeRTOS backend
 * Responsibility: Implements the OSAL API on top of the native FreeRTOS API, plus the
 *                 application hooks FreeRTOS requires (static idle task memory, stack overflow, assert).
 */
#include <atomic>

#include "osal/osal.h"

namespace {

std::atomic<bool> gShutdown{false};

TickType_t toTicks(uint32_t timeoutMs) {
    return (timeoutMs == osal::kWaitForever) ? portMAX_DELAY : pdMS_TO_TICKS(timeoutMs);
}

void taskTrampoline(void* param) {
    auto* ctrl = static_cast<osal::port::TaskControl*>(param);
    ctrl->entry(ctrl->arg);
    // A FreeRTOS task must never return from its function: delete it instead.
    vTaskDelete(nullptr);
}

} // namespace

namespace osal {

// --- Kernel --------------------------------------------------------------------------------------

void startScheduler() {
    vTaskStartScheduler();
    // Only reached if the idle task could not be created.
    for (;;) {
    }
}

void requestShutdown() {
    gShutdown.store(true);
}
bool shutdownRequested() {
    return gShutdown.load();
}

// --- Time ----------------------------------------------------------------------------------------

static_assert(configTICK_RATE_HZ == 1000, "OSAL assumes 1 tick == 1 ms");

uint32_t nowMs() {
    return static_cast<uint32_t>(xTaskGetTickCount());
}

void delayMs(uint32_t ms) {
    vTaskDelay(toTicks(ms));
}

void delayUntil(uint32_t& lastWakeMs, uint32_t periodMs) {
    TickType_t lastWake = static_cast<TickType_t>(lastWakeMs);
    xTaskDelayUntil(&lastWake, toTicks(periodMs));
    lastWakeMs = static_cast<uint32_t>(lastWake);
}

// --- Queue ---------------------------------------------------------------------------------------

ErrorCode QueueBase::createImpl(uint8_t* storage, size_t itemSize, size_t capacity) {
    ctrl_.handle = xQueueCreateStatic(static_cast<UBaseType_t>(capacity), static_cast<UBaseType_t>(itemSize), storage,
                                      &ctrl_.buffer);
    return (ctrl_.handle != nullptr) ? ErrorCode::OK : ErrorCode::NOT_INITIALIZED;
}

size_t QueueBase::count() const {
    return static_cast<size_t>(uxQueueMessagesWaiting(ctrl_.handle));
}

ErrorCode QueueBase::sendImpl(const void* item, uint32_t timeoutMs) {
    if (ctrl_.handle == nullptr) {
        return ErrorCode::NOT_INITIALIZED;
    }
    return (xQueueSend(ctrl_.handle, item, toTicks(timeoutMs)) == pdPASS) ? ErrorCode::OK : ErrorCode::QUEUE_FULL;
}

ErrorCode QueueBase::sendFromIsrImpl(const void* item) {
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    const BaseType_t result = xQueueSendFromISR(ctrl_.handle, item, &higherPriorityTaskWoken);
    portYIELD_FROM_ISR(higherPriorityTaskWoken);
    return (result == pdPASS) ? ErrorCode::OK : ErrorCode::QUEUE_FULL;
}

ErrorCode QueueBase::receiveImpl(void* item, uint32_t timeoutMs) {
    if (ctrl_.handle == nullptr) {
        return ErrorCode::NOT_INITIALIZED;
    }
    return (xQueueReceive(ctrl_.handle, item, toTicks(timeoutMs)) == pdPASS) ? ErrorCode::OK : ErrorCode::TIMEOUT;
}

// --- Binary semaphore ----------------------------------------------------------------------------

ErrorCode BinarySemaphore::create() {
    ctrl_.handle = xSemaphoreCreateBinaryStatic(&ctrl_.buffer);
    return (ctrl_.handle != nullptr) ? ErrorCode::OK : ErrorCode::NOT_INITIALIZED;
}

void BinarySemaphore::give() {
    xSemaphoreGive(ctrl_.handle);
}

void BinarySemaphore::giveFromIsr() {
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(ctrl_.handle, &higherPriorityTaskWoken);
    portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

ErrorCode BinarySemaphore::take(uint32_t timeoutMs) {
    if (ctrl_.handle == nullptr) {
        return ErrorCode::NOT_INITIALIZED;
    }
    return (xSemaphoreTake(ctrl_.handle, toTicks(timeoutMs)) == pdPASS) ? ErrorCode::OK : ErrorCode::TIMEOUT;
}

// --- Mutex ---------------------------------------------------------------------------------------

ErrorCode Mutex::create() {
    ctrl_.handle = xSemaphoreCreateMutexStatic(&ctrl_.buffer);
    return (ctrl_.handle != nullptr) ? ErrorCode::OK : ErrorCode::NOT_INITIALIZED;
}

void Mutex::lock() {
    xSemaphoreTake(ctrl_.handle, portMAX_DELAY);
}
void Mutex::unlock() {
    xSemaphoreGive(ctrl_.handle);
}

// --- Task ----------------------------------------------------------------------------------------

ErrorCode TaskBase::startImpl(const char* name, Priority priority, port::StackType* stack, size_t stackWords,
                              TaskEntry entry, void* arg) {
    static_assert(static_cast<UBaseType_t>(Priority::Realtime) < configMAX_PRIORITIES,
                  "configMAX_PRIORITIES too small for osal::Priority");
    if (entry == nullptr) {
        return ErrorCode::INVALID_DATA;
    }
    ctrl_.entry = entry;
    ctrl_.arg = arg;
    ctrl_.handle = xTaskCreateStatic(taskTrampoline, name, static_cast<configSTACK_DEPTH_TYPE>(stackWords), &ctrl_,
                                     static_cast<UBaseType_t>(priority), stack, &ctrl_.tcb);
    return (ctrl_.handle != nullptr) ? ErrorCode::OK : ErrorCode::NOT_INITIALIZED;
}

size_t TaskBase::stackHighWaterMarkWords() const {
    return (ctrl_.handle != nullptr) ? static_cast<size_t>(uxTaskGetStackHighWaterMark(ctrl_.handle)) : 0U;
}

} // namespace osal

// --- FreeRTOS application hooks ------------------------------------------------------------------

extern "C" {

// With static allocation the application provides the idle task's TCB and stack.
void vApplicationGetIdleTaskMemory(StaticTask_t** ppxIdleTaskTCBBuffer, StackType_t** ppxIdleTaskStackBuffer,
                                   configSTACK_DEPTH_TYPE* puxIdleTaskStackSize) {
    static StaticTask_t idleTcb;
    static StackType_t idleStack[configMINIMAL_STACK_SIZE];
    *ppxIdleTaskTCBBuffer = &idleTcb;
    *ppxIdleTaskStackBuffer = idleStack;
    *puxIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

// Called by the kernel (configCHECK_FOR_STACK_OVERFLOW=2) on a context switch that detects a
// corrupted stack. The system state can no longer be trusted: stop here (the watchdog of M8 will
// turn this into a reset).
void vApplicationStackOverflowHook(TaskHandle_t /*xTask*/, char* /*pcTaskName*/) {
    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}

void vAssertCalled(const char* /*file*/, int /*line*/) {
    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}

} // extern "C"
