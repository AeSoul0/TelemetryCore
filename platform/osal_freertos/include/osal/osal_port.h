/**
 * platform/osal_freertos/include/osal/osal_port.h
 * * Architecture Layer: Platform / OSAL FreeRTOS backend
 * Responsibility: Control blocks of the FreeRTOS backend. Every kernel object is created with the
 *                 *Static API, so the kernel never touches a heap (configSUPPORT_DYNAMIC_ALLOCATION=0).
 */
#pragma once
#include <cstddef>
#include <cstdint>

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

namespace osal::port {

using StackType = StackType_t;
using TaskEntry = void (*)(void* arg);

struct QueueControl {
    StaticQueue_t buffer;
    QueueHandle_t handle;
};

struct SemaphoreControl {
    StaticSemaphore_t buffer;
    SemaphoreHandle_t handle;
};

struct MutexControl {
    StaticSemaphore_t buffer;
    SemaphoreHandle_t handle;
};

struct TaskControl {
    StaticTask_t tcb;
    TaskHandle_t handle;
    TaskEntry entry;
    void* arg;
};

} // namespace osal::port
