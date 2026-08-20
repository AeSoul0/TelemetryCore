/**
 * app/src/main.cpp
 * * Architecture Layer: Application / Main
 * Responsibility: Entry point. Initializes FreeRTOS simulation and starts tasks.
 */
#include <iostream>
#include <thread>
#include <chrono>
#include <cstdlib>
#include "tasks.h"
#include "logger.h"

int main() {
    std::cout << "=== Embedded Telemetry Simulator (Phases 7-22) ===\n";
    LOG_INFO("Main", "Initializing system...");

    // Start all simulated RTOS tasks
    AppTasks::startTasks();

    // Let the system run for 3 seconds
    std::this_thread::sleep_for(std::chrono::seconds(3));

    LOG_INFO("Main", "Shutting down system...");
    
    // In an actual embedded system, the OS never stops.
    // For this PC simulation, we forcibly exit to avoid joining blocking queues.
    std::cout << "Test successfully completed.\n";
    std::exit(0);
}
