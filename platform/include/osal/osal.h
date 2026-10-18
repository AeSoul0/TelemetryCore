/**
 * platform/include/osal/osal.h
 * * Architecture Layer: Platform / OS Abstraction Layer (OSAL)
 * Responsibility: Single RTOS API used by the application. Two backends implement it:
 *   - platform/osal_host     : std::thread based simulation (PC, unit tests)
 *   - platform/osal_freertos : native FreeRTOS (STM32)
 *
 * Design rules:
 *   - No dynamic allocation: every queue, task stack and control block lives inside the object.
 *   - Objects are created explicitly with create()/start(), never in constructors. On FreeRTOS this
 *     keeps kernel calls out of static initialisation (before main(), HAL and clocks are ready).
 *   - Timeouts are in milliseconds. kNoWait polls, kWaitForever blocks until success or shutdown.
 *   - Functions with the FromIsr suffix are the only ones that may be called from an interrupt.
 */
#pragma once
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "error_codes.h"
#include "osal/osal_port.h" // backend specific control blocks (osal_host or osal_freertos)

namespace osal {

constexpr uint32_t kNoWait = 0;
constexpr uint32_t kWaitForever = UINT32_MAX;

// Maps 1:1 to FreeRTOS priorities (configMAX_PRIORITIES must be > Realtime).
enum class Priority : uint8_t { Idle = 0, Low, BelowNormal, Normal, AboveNormal, High, Realtime };

// ---------------------------------------------------------------------------------------------
// Kernel
// ---------------------------------------------------------------------------------------------

// FreeRTOS: starts the scheduler and never returns.
// Host: releases all created tasks and returns immediately.
void startScheduler();

// Asks every task loop to terminate. On the host all blocked calls wake up and return
// SYSTEM_SHUTDOWN; on FreeRTOS tasks notice it at their next loop iteration.
void requestShutdown();
bool shutdownRequested();

// ---------------------------------------------------------------------------------------------
// Time (1 tick = 1 ms)
// ---------------------------------------------------------------------------------------------

uint32_t nowMs();
void delayMs(uint32_t ms);
// Absolute periodic delay: wakes at lastWakeMs + periodMs and updates lastWakeMs. Unlike a
// relative delay, the period does not drift when the work inside the loop takes variable time.
void delayUntil(uint32_t& lastWakeMs, uint32_t periodMs);

// ---------------------------------------------------------------------------------------------
// Queue: fixed capacity, items copied byte-wise (like FreeRTOS queues).
// ---------------------------------------------------------------------------------------------

class QueueBase {
  public:
    QueueBase(const QueueBase&) = delete;
    QueueBase& operator=(const QueueBase&) = delete;

    size_t count() const;

  protected:
    QueueBase() = default;
    ~QueueBase() = default;

    ErrorCode createImpl(uint8_t* storage, size_t itemSize, size_t capacity);
    ErrorCode sendImpl(const void* item, uint32_t timeoutMs);
    ErrorCode sendFromIsrImpl(const void* item);
    ErrorCode receiveImpl(void* item, uint32_t timeoutMs);

  private:
    port::QueueControl ctrl_{};
};

template <typename T, size_t N> class Queue : public QueueBase {
    static_assert(std::is_trivially_copyable_v<T>, "Queue items are copied byte-wise");
    static_assert(N > 0, "Queue capacity must be at least 1");

  public:
    Queue() = default;

    ErrorCode create() { return createImpl(storage_, sizeof(T), N); }
    // Returns QUEUE_FULL if no slot became free within the timeout.
    ErrorCode send(const T& item, uint32_t timeoutMs = kNoWait) { return sendImpl(&item, timeoutMs); }
    ErrorCode sendFromIsr(const T& item) { return sendFromIsrImpl(&item); }
    // Returns TIMEOUT if nothing arrived within the timeout.
    ErrorCode receive(T& item, uint32_t timeoutMs = kWaitForever) { return receiveImpl(&item, timeoutMs); }

    static constexpr size_t capacity() { return N; }

  private:
    alignas(T) uint8_t storage_[sizeof(T) * N]{};
};

// ---------------------------------------------------------------------------------------------
// Synchronisation
// ---------------------------------------------------------------------------------------------

// Binary semaphore for signalling (ISR -> task). Not for protecting resources: use Mutex.
class BinarySemaphore {
  public:
    BinarySemaphore() = default;
    ~BinarySemaphore() = default;
    BinarySemaphore(const BinarySemaphore&) = delete;
    BinarySemaphore& operator=(const BinarySemaphore&) = delete;

    ErrorCode create();
    void give();
    void giveFromIsr();
    ErrorCode take(uint32_t timeoutMs = kWaitForever);

  private:
    port::SemaphoreControl ctrl_{};
};

// Mutex with priority inheritance on FreeRTOS. Never lock it from an ISR.
class Mutex {
  public:
    Mutex() = default;
    ~Mutex() = default;
    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;

    ErrorCode create();
    void lock();
    void unlock();

  private:
    port::MutexControl ctrl_{};
};

class LockGuard {
  public:
    explicit LockGuard(Mutex& mutex) : mutex_(mutex) { mutex_.lock(); }
    ~LockGuard() { mutex_.unlock(); }
    LockGuard(const LockGuard&) = delete;
    LockGuard& operator=(const LockGuard&) = delete;

  private:
    Mutex& mutex_;
};

// ---------------------------------------------------------------------------------------------
// Tasks: statically allocated stack. The entry function may return; the task is then deleted.
// ---------------------------------------------------------------------------------------------

using TaskEntry = port::TaskEntry;

class TaskBase {
  public:
    TaskBase(const TaskBase&) = delete;
    TaskBase& operator=(const TaskBase&) = delete;

    // Minimum free stack ever observed, in words (0 on the host, where it is not measurable).
    size_t stackHighWaterMarkWords() const;

  protected:
    TaskBase() = default;
    ~TaskBase() = default;

    ErrorCode startImpl(const char* name, Priority priority, port::StackType* stack, size_t stackWords, TaskEntry entry,
                        void* arg);

  private:
    port::TaskControl ctrl_{};
};

template <size_t StackWords> class Task : public TaskBase {
  public:
    Task() = default;

    ErrorCode start(const char* name, Priority priority, TaskEntry entry, void* arg = nullptr) {
        return startImpl(name, priority, stack_, StackWords, entry, arg);
    }

    static constexpr size_t stackWords() { return StackWords; }

  private:
    port::StackType stack_[StackWords]{};
};

} // namespace osal
