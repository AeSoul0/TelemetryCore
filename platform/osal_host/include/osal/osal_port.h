/**
 * platform/osal_host/include/osal/osal_port.h
 * * Architecture Layer: Platform / OSAL host backend
 * Responsibility: Control blocks of the PC simulation backend.
 *
 * The host backend behaves like a single-core kernel: queue and semaphore state is protected by
 * one global "kernel lock" and every blocked caller waits on one global condition variable.
 * That is less efficient than per-object locking, but it is simple, and it lets
 * requestShutdown() wake every blocked call at once.
 */
#pragma once
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <thread>

namespace osal::port {

using StackType = uint32_t;
using TaskEntry = void (*)(void* arg);

struct QueueControl {
    uint8_t* storage = nullptr;
    size_t itemSize = 0;
    size_t capacity = 0;
    size_t head = 0;
    size_t count = 0;
};

struct SemaphoreControl {
    bool created = false;
    bool available = false;
};

struct MutexControl {
    std::mutex mutex;
};

struct TaskControl {
    std::thread thread;
    TaskEntry entry = nullptr;
    void* arg = nullptr;
};

} // namespace osal::port
