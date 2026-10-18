/**
 * app/src/tasks.cpp
 * * Architecture Layer: Application / Tasks Implementation
 * Responsibility: Implements the system tasks and inter-task communication.
 *
 *   SensorTask (100 ms) --Q1--> ProcessingTask --Q2--> CommTask --> Uplink --> serial
 *   ISR --semaphore--> EventTask
 *
 * Every kernel object is statically allocated in this file; app::start() creates them.
 */
#include <algorithm>
#include <atomic>
#include <cstdio>

#include "acquisition.h"
#include "app.h"
#include "frame.h"
#include "logger.h"
#include "osal/osal.h"
#include "telemetry.h"
#include "uplink.h"

#ifndef TELEMETRY_FW_VERSION
#define TELEMETRY_FW_VERSION "0.0.0-dev"
#endif

namespace {

// --- Configuration -------------------------------------------------------------------------------

constexpr uint32_t kSensorPeriodMs = 100;
constexpr uint32_t kStatsPeriodMs = 5000;
constexpr size_t kQueueDepth = 10;

// Stack sizes in words (4 bytes). Starting values, to be validated with the high-water marks (M9).
constexpr size_t kEventStackWords = 256;
constexpr size_t kSensorStackWords = 384;
constexpr size_t kProcessingStackWords = 384;
constexpr size_t kCommStackWords = 512;

// --- Kernel objects ------------------------------------------------------------------------------

osal::Queue<SensorSample, kQueueDepth> sensorQueue;       // Q1
osal::Queue<TelemetryPacket, kQueueDepth> telemetryQueue; // Q2
osal::BinarySemaphore eventSemaphore;

osal::Task<kEventStackWords> eventTask;
osal::Task<kSensorStackWords> sensorTask;
osal::Task<kProcessingStackWords> processingTask;
osal::Task<kCommStackWords> commTask;

// --- Shared state --------------------------------------------------------------------------------

EnvSensor* envSensor = nullptr;
BatteryGauge* batteryGauge = nullptr;
const char* boardName = "unknown";

Uplink uplink;
TelemetryProcessor processor; // owned by ProcessingTask

std::atomic<uint32_t> samplesDropped{0};
std::atomic<uint32_t> packetsDropped{0};
std::atomic<uint32_t> dropsSinceLastPacket{0};
std::atomic<uint32_t> externalEvents{0};

void uplinkLogSink(const char* payload) {
    uplink.sendPayload(payload);
}

void recordDrop(std::atomic<uint32_t>& counter) {
    counter.fetch_add(1);
    dropsSinceLastPacket.fetch_add(1);
}

// --- SensorTask: periodic acquisition ------------------------------------------------------------

void sensorTaskFn(void* /*arg*/) {
    Acquisition acquisition(*envSensor, *batteryGauge);
    uint32_t lastWakeMs = osal::nowMs();
    uint32_t nextStatsMs = lastWakeMs + kStatsPeriodMs;
    uint32_t maxLatenessMs = 0;
    uint32_t failedCycles = 0;

    while (!osal::shutdownRequested()) {
        const uint32_t nowMs = osal::nowMs();
        const uint32_t latenessMs = nowMs - lastWakeMs; // how late we woke up vs. the schedule
        maxLatenessMs = std::max(maxLatenessMs, latenessMs);

        // Log transitions only: during an outage the link must not be flooded every 100 ms.
        const SensorSample sample = acquisition.acquire(nowMs);
        if (sample.error != ErrorCode::OK) {
            if (failedCycles++ == 0) {
                LOG_WARN("SensorTask", "sensor not responding after %u attempts",
                         static_cast<unsigned>(sample.attempts));
            }
        } else {
            if (failedCycles > 0) {
                LOG_INFO("SensorTask", "sensor recovered after %lu failed cycles",
                         static_cast<unsigned long>(failedCycles));
                failedCycles = 0;
            }
            if (sample.attempts > 1) {
                LOG_DEBUG("SensorTask", "read recovered at attempt %u", static_cast<unsigned>(sample.attempts));
            }
        }

        const ErrorCode err = sensorQueue.send(sample, osal::kNoWait);
        if (err == ErrorCode::SYSTEM_SHUTDOWN) {
            break;
        }
        if (err == ErrorCode::QUEUE_FULL) {
            recordDrop(samplesDropped);
            LOG_ERROR("SensorTask", "sensor queue full, sample dropped");
        }

        if (static_cast<int32_t>(nowMs - nextStatsMs) >= 0) {
            LOG_INFO("SensorTask", "jitter max %lu ms, q1 %u/%u, dropped %lu/%lu, stack free %u/%u/%u words",
                     static_cast<unsigned long>(maxLatenessMs), static_cast<unsigned>(sensorQueue.count()),
                     static_cast<unsigned>(sensorQueue.capacity()), static_cast<unsigned long>(samplesDropped.load()),
                     static_cast<unsigned long>(packetsDropped.load()),
                     static_cast<unsigned>(sensorTask.stackHighWaterMarkWords()),
                     static_cast<unsigned>(processingTask.stackHighWaterMarkWords()),
                     static_cast<unsigned>(commTask.stackHighWaterMarkWords()));
            maxLatenessMs = 0;
            nextStatsMs += kStatsPeriodMs;
        }

        osal::delayUntil(lastWakeMs, kSensorPeriodMs);
    }
}

// --- ProcessingTask: validation, fault policy, packet creation -----------------------------------

void logStatusChange(SystemStatus from, const TelemetryPacket& packet) {
    const auto flags = static_cast<unsigned>(packet.faultFlags);
    switch (packet.status) {
    case SystemStatus::OK:
        LOG_INFO("ProcTask", "status %s -> OK", statusName(from));
        break;
    case SystemStatus::WARNING:
        LOG_WARN("ProcTask", "status %s -> WARNING, flags %02X", statusName(from), flags);
        break;
    case SystemStatus::CRITICAL:
        LOG_ERROR("ProcTask", "status %s -> CRITICAL, flags %02X", statusName(from), flags);
        break;
    }
}

void processingTaskFn(void* /*arg*/) {
    SystemStatus lastStatus = SystemStatus::OK;

    while (!osal::shutdownRequested()) {
        SensorSample sample{};
        // Block until data is available
        if (sensorQueue.receive(sample, osal::kWaitForever) != ErrorCode::OK) {
            continue; // SYSTEM_SHUTDOWN ends the loop through the condition
        }

        const TelemetryPacket packet = processor.process(sample, dropsSinceLastPacket.exchange(0));
        if (packet.status != lastStatus) {
            logStatusChange(lastStatus, packet);
            lastStatus = packet.status;
        }

        const ErrorCode err = telemetryQueue.send(packet, osal::kNoWait);
        if (err == ErrorCode::QUEUE_FULL) {
            recordDrop(packetsDropped);
            LOG_ERROR("ProcTask", "telemetry queue full, packet dropped");
        }
    }
}

// --- CommTask: serialisation and transmission ----------------------------------------------------

void commTaskFn(void* /*arg*/) {
    char payload[frame::kMaxPayloadLength];

    // First frame after reset: identifies firmware and board to the ground station.
    if (std::snprintf(payload, sizeof(payload), "BOOT,fw=%s,board=%s", app::firmwareVersion(), boardName) > 0) {
        uplink.sendPayload(payload);
    }

    while (!osal::shutdownRequested()) {
        TelemetryPacket packet{};
        // Block until data is available
        if (telemetryQueue.receive(packet, osal::kWaitForever) != ErrorCode::OK) {
            continue;
        }
        if (TelemetryProcessor::serialize(packet, payload, sizeof(payload)) == 0) {
            LOG_ERROR("CommTask", "serialization failed for packet %lu", static_cast<unsigned long>(packet.sequence));
            continue;
        }
        uplink.sendPayload(payload);
    }
}

// --- EventTask: deferred interrupt handling -------------------------------------------------------

void eventTaskFn(void* /*arg*/) {
    while (!osal::shutdownRequested()) {
        // Woken up by onExternalEventFromIsr()
        if (eventSemaphore.take(osal::kWaitForever) != ErrorCode::OK) {
            continue;
        }
        const uint32_t count = externalEvents.fetch_add(1) + 1;
        LOG_DEBUG("EventTask", "external event #%lu", static_cast<unsigned long>(count));
    }
}

} // namespace

namespace app {

const char* firmwareVersion() {
    return TELEMETRY_FW_VERSION;
}

ErrorCode start(const Dependencies& deps) {
    envSensor = &deps.envSensor;
    batteryGauge = &deps.batteryGauge;
    boardName = deps.boardName;

    const ErrorCode steps[] = {
        sensorQueue.create(),     telemetryQueue.create(), eventSemaphore.create(),
        uplink.init(deps.serial), deps.envSensor.init(),   deps.batteryGauge.init(),
    };
    for (const ErrorCode step : steps) {
        if (step != ErrorCode::OK) {
            return step;
        }
    }
    logger::setSink(uplinkLogSink);

    // Higher priority = runs first when several tasks are ready.
    const ErrorCode tasks[] = {
        eventTask.start("EventTask", osal::Priority::High, eventTaskFn),
        sensorTask.start("SensorTask", osal::Priority::AboveNormal, sensorTaskFn),
        processingTask.start("ProcTask", osal::Priority::Normal, processingTaskFn),
        commTask.start("CommTask", osal::Priority::BelowNormal, commTaskFn),
    };
    for (const ErrorCode task : tasks) {
        if (task != ErrorCode::OK) {
            return task;
        }
    }
    return ErrorCode::OK;
}

void onExternalEventFromIsr() {
    eventSemaphore.giveFromIsr();
}

Stats stats() {
    return Stats{samplesDropped.load(), packetsDropped.load(), externalEvents.load(), uplink.errorCount()};
}

} // namespace app
