// Integration test: the whole application (4 tasks, 2 queues, uplink) on the host OSAL.

#include <gtest/gtest.h>

#include <cstdlib>
#include <string>
#include <vector>

#include "app.h"
#include "frame.h"
#include "logger.h"
#include "mock_battery_gauge.h"
#include "mock_env_sensor.h"
#include "osal/host.h"
#include "osal/osal.h"
#include "test_support.h"

namespace {

std::vector<std::string> split(const std::string& text, char separator) {
    std::vector<std::string> fields;
    size_t start = 0;
    size_t end = 0;
    while ((end = text.find(separator, start)) != std::string::npos) {
        fields.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    fields.push_back(text.substr(start));
    return fields;
}

} // namespace

TEST(Pipeline, SensorToSerialEndToEnd) {
    osal::host::resetKernel();
    MockEnvSensor envSensor(MockEnvSensor::Config{false, 0, 0});
    MockBatteryGauge batteryGauge;
    CaptureSerial serial;

    ASSERT_EQ(app::start({envSensor, batteryGauge, serial, "test-board"}), ErrorCode::OK);
    osal::startScheduler();
    osal::delayMs(80);
    app::onExternalEventFromIsr();
    osal::delayMs(470);
    osal::requestShutdown();
    osal::host::joinAllTasks();
    logger::setSink(nullptr);

    const std::vector<std::string> lines = serial.lines();
    ASSERT_FALSE(lines.empty());

    std::vector<std::vector<std::string>> telemetry;
    bool sawBoot = false;
    bool sawEvent = false;
    for (const std::string& line : lines) {
        char payload[frame::kMaxPayloadLength];
        ASSERT_TRUE(frame::decode(line.c_str(), payload, sizeof(payload))) << "bad frame: " << line;
        const std::vector<std::string> fields = split(payload, ',');
        if (fields[0] == "BOOT") {
            sawBoot = true;
            EXPECT_EQ(fields.at(2), "board=test-board");
        } else if (fields[0] == "TLM") {
            telemetry.push_back(fields);
        } else if (fields[0] == "LOG" && fields.at(2) == "EventTask") {
            sawEvent = true;
        }
    }
    EXPECT_TRUE(lines.front().rfind("$BOOT,", 0) == 0) << "BOOT is the first frame";
    EXPECT_TRUE(sawBoot);
    EXPECT_TRUE(sawEvent) << "ISR -> semaphore -> EventTask";

    // ~550 ms at 100 ms period -> 5 or 6 packets, consecutive sequence numbers, rising timestamps.
    ASSERT_GE(telemetry.size(), 4U);
    ASSERT_LE(telemetry.size(), 7U);
    for (size_t i = 0; i < telemetry.size(); ++i) {
        ASSERT_EQ(telemetry[i].size(), 8U);
        EXPECT_EQ(std::strtoul(telemetry[i][1].c_str(), nullptr, 10), i);
        EXPECT_EQ(telemetry[i][6], "OK");
        EXPECT_EQ(telemetry[i][7], "00");
        if (i > 0) {
            const unsigned long previous = std::strtoul(telemetry[i - 1][2].c_str(), nullptr, 10);
            const unsigned long current = std::strtoul(telemetry[i][2].c_str(), nullptr, 10);
            EXPECT_NEAR(static_cast<double>(current - previous), 100.0, 15.0) << "period of SensorTask";
        }
    }

    const app::Stats stats = app::stats();
    EXPECT_EQ(stats.samplesDropped, 0U);
    EXPECT_EQ(stats.packetsDropped, 0U);
    EXPECT_EQ(stats.externalEvents, 1U);
    EXPECT_EQ(stats.uplinkErrors, 0U);
}
