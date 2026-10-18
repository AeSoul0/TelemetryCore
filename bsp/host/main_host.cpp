/**
 * bsp/host/main_host.cpp
 * * Architecture Layer: Board support / PC simulation entry point
 * Responsibility: Wires mock drivers into the application, simulates the hardware interrupt and
 *                 the sensor faults, and runs the system for a limited time.
 *
 * Usage: telemetry_sim [--duration-ms N] [--forever] [--no-faults]
 * Frames are written to stdout, e.g. pipe them into the ground station:
 *     telemetry_sim --forever | python tools/ground_station/ground_station.py --stdin
 */
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

#include "app.h"
#include "mock_battery_gauge.h"
#include "mock_env_sensor.h"
#include "osal/host.h"
#include "osal/osal.h"
#include "stdout_serial.h"

namespace {

constexpr uint32_t kIsrPeriodMs = 500;

struct Options {
    uint32_t durationMs = 3000;
    bool forever = false;
    bool faults = true;
};

MockEnvSensor envSensor;
MockBatteryGauge batteryGauge;
StdoutSerial serial;
osal::Task<256> isrSimulationTask;
bool injectFaults = true;

// Plays the role of a hardware timer interrupt, and of a flaky sensor connection:
//   - every 500 ms: external event (semaphore given "from ISR")
//   - every 3 s:    one failed read   -> recovered by the retry, no status change
//   - every 15 s:   a 1.5 s outage    -> WARNING, then CRITICAL, then recovery
void isrSimulationFn(void* /*arg*/) {
    uint32_t lastWakeMs = osal::nowMs();
    uint32_t tick = 0;
    while (!osal::shutdownRequested()) {
        osal::delayUntil(lastWakeMs, kIsrPeriodMs);
        ++tick;
        app::onExternalEventFromIsr();
        if (!injectFaults) {
            continue;
        }
        if (tick % 30 == 0) {
            envSensor.setDisconnected(true);
        } else if (tick % 30 == 3) {
            envSensor.setDisconnected(false);
        } else if (tick % 6 == 0) {
            envSensor.injectFailures(1);
        }
    }
}

bool parseOptions(int argc, char** argv, Options& options) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--duration-ms") == 0 && i + 1 < argc) {
            options.durationMs = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
        } else if (std::strcmp(argv[i], "--forever") == 0) {
            options.forever = true;
        } else if (std::strcmp(argv[i], "--no-faults") == 0) {
            options.faults = false;
        } else {
            (void)std::fprintf(stderr, "usage: %s [--duration-ms N] [--forever] [--no-faults]\n", argv[0]);
            return false;
        }
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    Options options;
    if (!parseOptions(argc, argv, options)) {
        return EXIT_FAILURE;
    }
    injectFaults = options.faults;

    const app::Dependencies deps{envSensor, batteryGauge, serial, "host-sim"};
    const ErrorCode result = app::start(deps);
    if (result != ErrorCode::OK) {
        (void)std::fprintf(stderr, "app::start failed: %s\n", errorCodeName(result));
        return EXIT_FAILURE;
    }
    if (isrSimulationTask.start("IsrSim", osal::Priority::Realtime, isrSimulationFn) != ErrorCode::OK) {
        return EXIT_FAILURE;
    }

    osal::startScheduler(); // host: returns immediately, tasks are now running

    if (options.forever) {
        for (;;) {
            std::this_thread::sleep_for(std::chrono::hours(1));
        }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(options.durationMs));

    // Graceful shutdown: wake every blocked task, then wait for all of them.
    osal::requestShutdown();
    osal::host::joinAllTasks();

    const app::Stats stats = app::stats();
    (void)std::fprintf(
        stderr, "simulation finished: %lu events, %lu samples dropped, %lu packets dropped, %lu uplink errors\n",
        static_cast<unsigned long>(stats.externalEvents), static_cast<unsigned long>(stats.samplesDropped),
        static_cast<unsigned long>(stats.packetsDropped), static_cast<unsigned long>(stats.uplinkErrors));
    return EXIT_SUCCESS;
}
