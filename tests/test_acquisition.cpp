#include <gtest/gtest.h>

#include "acquisition.h"
#include "test_support.h"

TEST(Acquisition, FirstAttemptSucceeds) {
    ScriptedEnvSensor sensor({{ErrorCode::OK, 23.4f, 1013.0f}});
    FixedBatteryGauge battery(80.0f);
    Acquisition acquisition(sensor, battery);

    const SensorSample sample = acquisition.acquire(500);
    EXPECT_EQ(sample.error, ErrorCode::OK);
    EXPECT_EQ(sample.attempts, 1);
    EXPECT_EQ(sample.reading.timestampMs, 500U);
    EXPECT_FLOAT_EQ(sample.reading.temperatureC, 23.4f);
    EXPECT_FLOAT_EQ(sample.reading.pressureHpa, 1013.0f);
    EXPECT_FLOAT_EQ(sample.batteryPct, 80.0f);
}

TEST(Acquisition, TransientFailureIsRetried) {
    ScriptedEnvSensor sensor(
        {{ErrorCode::SENSOR_ERROR, 0, 0}, {ErrorCode::SENSOR_ERROR, 0, 0}, {ErrorCode::OK, 24.0f, 1000.0f}});
    FixedBatteryGauge battery(80.0f);
    Acquisition acquisition(sensor, battery);

    const SensorSample sample = acquisition.acquire(0);
    EXPECT_EQ(sample.error, ErrorCode::OK);
    EXPECT_EQ(sample.attempts, 3);
    EXPECT_FLOAT_EQ(sample.reading.temperatureC, 24.0f);
}

TEST(Acquisition, PersistentFailureReportsSensorErrorAfterMaxAttempts) {
    ScriptedEnvSensor sensor({{ErrorCode::SENSOR_ERROR, 0, 0}});
    FixedBatteryGauge battery(80.0f);
    Acquisition acquisition(sensor, battery, 3);

    const SensorSample sample = acquisition.acquire(0);
    EXPECT_EQ(sample.error, ErrorCode::SENSOR_ERROR);
    EXPECT_EQ(sample.attempts, 3);
    EXPECT_EQ(sensor.reads, 3) << "no more than maxAttempts reads";
}

TEST(Acquisition, ZeroAttemptsMeansOne) {
    ScriptedEnvSensor sensor({{ErrorCode::SENSOR_ERROR, 0, 0}});
    FixedBatteryGauge battery(80.0f);
    Acquisition acquisition(sensor, battery, 0);
    acquisition.acquire(0);
    EXPECT_EQ(sensor.reads, 1);
}
