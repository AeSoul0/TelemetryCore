/**
 * platform/include/rtos_sim.h
 * * Architecture Layer: Platform / OS Abstraction
 * Responsibility: Simulates FreeRTOS primitives (Task, Queue, Mutex, Semaphore) on a PC.
 */
#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <chrono>
#include <atomic>
#include "error_codes.h"

namespace RTOS {

    // Mutex
    class Mutex {
    private:
        std::mutex mtx;
    public:
        void lock() { mtx.lock(); }
        void unlock() { mtx.unlock(); }
    };

    // Semaphore
    class Semaphore {
    private:
        std::mutex mtx;
        std::condition_variable cv;
        bool count = false;
        bool stopped = false;
    public:
        void give() {
            std::lock_guard<std::mutex> lock(mtx);
            count = true;
            cv.notify_one();
        }
        ErrorCode take() {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [this]() { return count || stopped; });
            if (stopped) return ErrorCode::SYSTEM_SHUTDOWN;
            count = false;
            return ErrorCode::OK;
        }
        void abort() {
            std::lock_guard<std::mutex> lock(mtx);
            stopped = true;
            cv.notify_all();
        }
    };

    // Queue
    template<typename T>
    class Queue {
    private:
        std::queue<T> q;
        std::mutex mtx;
        std::condition_variable cv;
        size_t maxSize;
        bool stopped = false;
    public:
        Queue(size_t max_size = 10) : maxSize(max_size) {}
        
        ErrorCode send(const T& item) {
            std::lock_guard<std::mutex> lock(mtx);
            if (stopped) return ErrorCode::SYSTEM_SHUTDOWN;
            if (q.size() >= maxSize) {
                return ErrorCode::QUEUE_FULL;
            }
            q.push(item);
            cv.notify_one();
            return ErrorCode::OK;
        }

        ErrorCode receive(T& item) {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [this]() { return !q.empty() || stopped; });
            if (stopped) return ErrorCode::SYSTEM_SHUTDOWN;
            item = q.front();
            q.pop();
            return ErrorCode::OK;
        }
        
        void abort() {
            std::lock_guard<std::mutex> lock(mtx);
            stopped = true;
            cv.notify_all();
        }
    };

    // Task
    class Task {
    private:
        std::thread worker;
        std::function<void()> taskFunction;
        std::atomic<bool> running;
        
        void run() {
            while (running) {
                taskFunction();
            }
        }
    public:
        Task(std::function<void()> func) : taskFunction(func), running(false) {}
        
        ~Task() { stop(); }
        
        void start() {
            if (!running) {
                running = true;
                worker = std::thread(&Task::run, this);
            }
        }
        
        void stop() {
            if (running) {
                running = false;
                if (worker.joinable()) {
                    worker.join();
                }
            }
        }
    };

    // Tick delay wrapper (1 ms per tick)
    inline void delay(uint32_t ticks) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ticks));
    }
}
