/**
 * platform/osal_host/include/osal/host.h
 * * Architecture Layer: Platform / OSAL host backend
 * Responsibility: Host-only kernel controls used by the PC entry point and by unit tests.
 *                 Never include this from app/: these functions do not exist on the STM32.
 */
#pragma once

namespace osal::host {

// Joins every task thread started so far. Call after osal::requestShutdown().
void joinAllTasks();

// Restores a fresh kernel state (scheduler stopped, no shutdown, clock at 0).
// Only valid once all tasks have been joined. Lets tests run several scenarios in one process.
void resetKernel();

} // namespace osal::host
