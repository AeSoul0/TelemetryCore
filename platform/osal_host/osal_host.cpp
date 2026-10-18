/**
 * platform/osal_host/osal_host.cpp
 * * Architecture Layer: Platform / OSAL host backend
 * Responsibility: Implements the OSAL API with std::thread, std::mutex and std::condition_variable.
 */
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>

#include "osal/host.h"
#include "osal/osal.h"

namespace {

using Clock = std::chrono::steady_clock;

std::mutex gKernelMutex;
std::condition_variable gKernelCv;
bool gSchedulerStarted = false;
std::atomic<bool> gShutdown{false};
Clock::time_point gEpoch = Clock::now();

constexpr size_t kMaxTasks = 16;
std::array<osal::port::TaskControl*, kMaxTasks> gTasks{};
size_t gTaskCount = 0;

// Waits on the kernel condition variable until pred() holds, the timeout expires or a shutdown
// is requested. Returns pred() at the end; callers check gShutdown themselves.
template <typename Pred> bool waitFor(std::unique_lock<std::mutex>& lock, uint32_t timeoutMs, Pred pred) {
    auto stopOrReady = [&pred]() { return gShutdown.load() || pred(); };
    if (timeoutMs == osal::kWaitForever) {
        gKernelCv.wait(lock, stopOrReady);
    } else if (timeoutMs != osal::kNoWait) {
        gKernelCv.wait_for(lock, std::chrono::milliseconds(timeoutMs), stopOrReady);
    }
    return pred();
}

void taskTrampoline(osal::port::TaskControl* ctrl) {
    {
        std::unique_lock<std::mutex> lock(gKernelMutex);
        gKernelCv.wait(lock, []() { return gSchedulerStarted || gShutdown.load(); });
    }
    if (!gShutdown.load()) {
        ctrl->entry(ctrl->arg);
    }
}

} // namespace

namespace osal {

// --- Kernel --------------------------------------------------------------------------------------

void startScheduler() {
    std::lock_guard<std::mutex> lock(gKernelMutex);
    gSchedulerStarted = true;
    gKernelCv.notify_all();
}

void requestShutdown() {
    std::lock_guard<std::mutex> lock(gKernelMutex);
    gShutdown.store(true);
    gKernelCv.notify_all();
}

bool shutdownRequested() {
    return gShutdown.load();
}

// --- Time ----------------------------------------------------------------------------------------

uint32_t nowMs() {
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - gEpoch);
    return static_cast<uint32_t>(elapsed.count());
}

void delayMs(uint32_t ms) {
    std::unique_lock<std::mutex> lock(gKernelMutex);
    gKernelCv.wait_for(lock, std::chrono::milliseconds(ms), []() { return gShutdown.load(); });
}

void delayUntil(uint32_t& lastWakeMs, uint32_t periodMs) {
    lastWakeMs += periodMs;
    const auto wakeTime = gEpoch + std::chrono::milliseconds(lastWakeMs);
    std::unique_lock<std::mutex> lock(gKernelMutex);
    // If the deadline is already in the past this returns immediately (same as FreeRTOS).
    gKernelCv.wait_until(lock, wakeTime, []() { return gShutdown.load(); });
}

// --- Queue ---------------------------------------------------------------------------------------

ErrorCode QueueBase::createImpl(uint8_t* storage, size_t itemSize, size_t capacity) {
    std::lock_guard<std::mutex> lock(gKernelMutex);
    ctrl_.storage = storage;
    ctrl_.itemSize = itemSize;
    ctrl_.capacity = capacity;
    ctrl_.head = 0;
    ctrl_.count = 0;
    return ErrorCode::OK;
}

size_t QueueBase::count() const {
    std::lock_guard<std::mutex> lock(gKernelMutex);
    return ctrl_.count;
}

ErrorCode QueueBase::sendImpl(const void* item, uint32_t timeoutMs) {
    std::unique_lock<std::mutex> lock(gKernelMutex);
    if (ctrl_.storage == nullptr) {
        return ErrorCode::NOT_INITIALIZED;
    }
    const bool hasSpace = waitFor(lock, timeoutMs, [this]() { return ctrl_.count < ctrl_.capacity; });
    if (gShutdown.load()) {
        return ErrorCode::SYSTEM_SHUTDOWN;
    }
    if (!hasSpace) {
        return ErrorCode::QUEUE_FULL;
    }
    const size_t tail = (ctrl_.head + ctrl_.count) % ctrl_.capacity;
    std::memcpy(ctrl_.storage + (tail * ctrl_.itemSize), item, ctrl_.itemSize);
    ++ctrl_.count;
    gKernelCv.notify_all();
    return ErrorCode::OK;
}

// On the host an "ISR" is just another thread, so a non-blocking send is equivalent.
ErrorCode QueueBase::sendFromIsrImpl(const void* item) {
    return sendImpl(item, kNoWait);
}

ErrorCode QueueBase::receiveImpl(void* item, uint32_t timeoutMs) {
    std::unique_lock<std::mutex> lock(gKernelMutex);
    if (ctrl_.storage == nullptr) {
        return ErrorCode::NOT_INITIALIZED;
    }
    const bool hasItem = waitFor(lock, timeoutMs, [this]() { return ctrl_.count > 0; });
    if (gShutdown.load()) {
        return ErrorCode::SYSTEM_SHUTDOWN;
    }
    if (!hasItem) {
        return ErrorCode::TIMEOUT;
    }
    std::memcpy(item, ctrl_.storage + (ctrl_.head * ctrl_.itemSize), ctrl_.itemSize);
    ctrl_.head = (ctrl_.head + 1) % ctrl_.capacity;
    --ctrl_.count;
    gKernelCv.notify_all();
    return ErrorCode::OK;
}

// --- Binary semaphore ----------------------------------------------------------------------------

ErrorCode BinarySemaphore::create() {
    std::lock_guard<std::mutex> lock(gKernelMutex);
    ctrl_.created = true;
    ctrl_.available = false;
    return ErrorCode::OK;
}

void BinarySemaphore::give() {
    std::lock_guard<std::mutex> lock(gKernelMutex);
    ctrl_.available = true;
    gKernelCv.notify_all();
}

void BinarySemaphore::giveFromIsr() {
    give();
}

ErrorCode BinarySemaphore::take(uint32_t timeoutMs) {
    std::unique_lock<std::mutex> lock(gKernelMutex);
    if (!ctrl_.created) {
        return ErrorCode::NOT_INITIALIZED;
    }
    const bool acquired = waitFor(lock, timeoutMs, [this]() { return ctrl_.available; });
    if (gShutdown.load()) {
        return ErrorCode::SYSTEM_SHUTDOWN;
    }
    if (!acquired) {
        return ErrorCode::TIMEOUT;
    }
    ctrl_.available = false;
    return ErrorCode::OK;
}

// --- Mutex ---------------------------------------------------------------------------------------

// NOLINTNEXTLINE(readability-convert-member-functions-to-static): OSAL API, stateful on FreeRTOS
ErrorCode Mutex::create() {
    return ErrorCode::OK;
}
void Mutex::lock() {
    ctrl_.mutex.lock();
}
void Mutex::unlock() {
    ctrl_.mutex.unlock();
}

// --- Task ----------------------------------------------------------------------------------------

ErrorCode TaskBase::startImpl(const char* /*name*/, Priority /*priority*/, port::StackType* /*stack*/,
                              size_t /*stackWords*/, TaskEntry entry, void* arg) {
    std::lock_guard<std::mutex> lock(gKernelMutex);
    if (entry == nullptr || ctrl_.thread.joinable() || gTaskCount >= kMaxTasks) {
        return ErrorCode::INVALID_DATA;
    }
    ctrl_.entry = entry;
    ctrl_.arg = arg;
    ctrl_.thread = std::thread(taskTrampoline, &ctrl_);
    gTasks[gTaskCount++] = &ctrl_;
    return ErrorCode::OK;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static): OSAL API, stateful on FreeRTOS
size_t TaskBase::stackHighWaterMarkWords() const {
    return 0;
}

// --- Host-only controls --------------------------------------------------------------------------

namespace host {

void joinAllTasks() {
    std::array<port::TaskControl*, kMaxTasks> tasks{};
    size_t count = 0;
    {
        std::lock_guard<std::mutex> lock(gKernelMutex);
        tasks = gTasks;
        count = gTaskCount;
        gTaskCount = 0;
    }
    for (size_t i = 0; i < count; ++i) {
        if (tasks[i]->thread.joinable()) {
            tasks[i]->thread.join();
        }
    }
}

void resetKernel() {
    std::lock_guard<std::mutex> lock(gKernelMutex);
    gSchedulerStarted = false;
    gShutdown.store(false);
    gEpoch = Clock::now();
}

} // namespace host

} // namespace osal
