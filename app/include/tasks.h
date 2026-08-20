/**
 * app/include/tasks.h
 * * Architecture Layer: Application / Tasks
 * Responsibility: Defines the FreeRTOS tasks (Sensor, Processing, Communication).
 */
#pragma once

namespace AppTasks {
    void startTasks();
    void stopTasks();
}
