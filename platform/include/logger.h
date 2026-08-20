/**
 * platform/include/logger.h
 * * Architecture Layer: Platform / Logging
 * Responsibility: Provides standardized logging macros across the system.
 */
#pragma once
#include <iostream>
#include <string>

// Logging
namespace Logger {
    enum class Level {
        DEBUG,
        INFO,
        WARN,
        ERROR
    };

    inline void log(Level level, const std::string& component, const std::string& message) {
        // In a real RTOS, this std::cout must be protected by a Mutex!
        // We will add the Mutex protection when we implement the RTOS simulation layer.
        switch(level) {
            case Level::DEBUG: std::cout << "[DEBUG]"; break;
            case Level::INFO:  std::cout << "[INFO] "; break;
            case Level::WARN:  std::cout << "[WARN] "; break;
            case Level::ERROR: std::cout << "[ERROR]"; break;
        }
        std::cout << " [" << component << "] " << message << "\n";
    }
}

#define LOG_DEBUG(comp, msg) Logger::log(Logger::Level::DEBUG, comp, msg)
#define LOG_INFO(comp, msg)  Logger::log(Logger::Level::INFO, comp, msg)
#define LOG_WARN(comp, msg)  Logger::log(Logger::Level::WARN, comp, msg)
#define LOG_ERROR(comp, msg) Logger::log(Logger::Level::ERROR, comp, msg)
