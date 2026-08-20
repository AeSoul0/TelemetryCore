/**
 * app/src/tasks.cpp
 * * Architecture Layer: Application / Tasks Implementation
 * Responsibility: Implements the main system tasks and inter-task communication.
 */
#include "tasks.h"
#include "rtos_sim.h"
#include "sensor.h"
#include "telemetry.h"
#include "logger.h"
#include <memory>
#include <iostream>

// UART Mutex to protect std::cout
RTOS::Mutex uartMutex;

// Global Queues
RTOS::Queue<SensorReading> sensorQueue(10);
RTOS::Queue<TelemetryPacket> telemetryQueue(10);

// Global Hardware & Processing
MockTemperatureSensor temperatureSensor;
TelemetryProcessor telemetryProcessor;

// Global Tasks
std::unique_ptr<RTOS::Task> sensorTaskPtr;
std::unique_ptr<RTOS::Task> processingTaskPtr;
std::unique_ptr<RTOS::Task> commTaskPtr;
std::unique_ptr<RTOS::Task> isrTaskPtr;
std::unique_ptr<RTOS::Task> eventHandlerTaskPtr;

// Semaphore for Interrupt simulation
RTOS::Semaphore eventSemaphore;

// SensorTask
void sensorTaskFn() {
    static uint32_t timestamp = 0;
    
    SensorReading reading;
    reading.timestamp = timestamp;
    reading.temperature = temperatureSensor.readTemperature();
    
    // Send to queue
    ErrorCode err = sensorQueue.send(reading);
    if (err == ErrorCode::SYSTEM_SHUTDOWN) return;
    if (err == ErrorCode::QUEUE_FULL) {
        LOG_ERROR("SensorTask", "Queue 1 Full! Dropping reading.");
    }
    
    timestamp += 100;
    RTOS::delay(100); // 100ms period
}

// ProcessingTask
void processingTaskFn() {
    SensorReading reading;
    // Block until data is available
    if (sensorQueue.receive(reading) == ErrorCode::SYSTEM_SHUTDOWN) return;
    
    TelemetryPacket packet = telemetryProcessor.process(reading);
    
    ErrorCode err = telemetryQueue.send(packet);
    if (err == ErrorCode::SYSTEM_SHUTDOWN) return;
    if (err == ErrorCode::QUEUE_FULL) {
        LOG_ERROR("ProcTask", "Queue 2 Full! Dropping packet.");
    }
}

// CommunicationTask
void commTaskFn() {
    TelemetryPacket packet;
    // Block until data is available
    if (telemetryQueue.receive(packet) == ErrorCode::SYSTEM_SHUTDOWN) return;
    
    std::string serialized = telemetryProcessor.serialize(packet);
    
    // Protect shared resource (UART/stdout)
    uartMutex.lock();
    LOG_INFO("CommTask", serialized);
    uartMutex.unlock();
}

// Interrupt Simulation Task
void isrSimulationFn() {
    RTOS::delay(500); // Fire interrupt every 500ms
    eventSemaphore.give(); // Trigger event
}

// Event Handler Task (woken up by Semaphore)
void eventHandlerTaskFn() {
    if (eventSemaphore.take() == ErrorCode::SYSTEM_SHUTDOWN) return;
    uartMutex.lock();
    LOG_DEBUG("ISR_Handler", "Hardware interrupt triggered!");
    uartMutex.unlock();
}

namespace AppTasks {
    void startTasks() {
        LOG_INFO("System", "Starting FreeRTOS Simulation Tasks...");
        
        sensorTaskPtr = std::make_unique<RTOS::Task>(sensorTaskFn);
        processingTaskPtr = std::make_unique<RTOS::Task>(processingTaskFn);
        commTaskPtr = std::make_unique<RTOS::Task>(commTaskFn);
        isrTaskPtr = std::make_unique<RTOS::Task>(isrSimulationFn);
        eventHandlerTaskPtr = std::make_unique<RTOS::Task>(eventHandlerTaskFn);
        
        sensorTaskPtr->start();
        processingTaskPtr->start();
        commTaskPtr->start();
        isrTaskPtr->start();
        eventHandlerTaskPtr->start();
    }
    
    void stopTasks() {
        // Wake up all blocked primitives so tasks can exit gracefully
        sensorQueue.abort();
        telemetryQueue.abort();
        eventSemaphore.abort();
        
        sensorTaskPtr->stop();
        processingTaskPtr->stop();
        commTaskPtr->stop();
        isrTaskPtr->stop();
        eventHandlerTaskPtr->stop();
    }
}
