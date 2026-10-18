#include <gtest/gtest.h>

#include <cmath>

#include "mock_battery_gauge.h"
#include "mock_env_sensor.h"

// Roadmap phase 6.1: readTemperature() -> plausible value
TEST(MockEnvSensor, NormalReadingsArePlausible) {
    MockEnvSensor sensor(MockEnvSensor::Config{false, 0, 0});
    ASSERT_EQ(sensor.init(), ErrorCode::OK);
    for (int i = 0; i < 100; ++i) {
        SensorReading reading{};
        ASSERT_EQ(sensor.read(reading), ErrorCode::OK);
        EXPECT_GE(reading.temperatureC, 23.4f);
        EXPECT_LE(reading.temperatureC, 25.0f);
        EXPECT_NEAR(reading.pressureHpa, 1013.25f, 0.51f);
    }
}

TEST(MockEnvSensor, InjectsPeriodicAnomalies) {
    MockEnvSensor sensor(MockEnvSensor::Config{true, 4, 7});
    sensor.init();
    SensorReading reading{};
    for (int i = 1; i <= 28; ++i) {
        sensor.read(reading);
        if (i % 7 == 0) {
            EXPECT_FLOAT_EQ(reading.temperatureC, MockEnvSensor::kCriticalTempC) << "read " << i;
        } else if (i % 4 == 0) {
            EXPECT_FLOAT_EQ(reading.temperatureC, MockEnvSensor::kWarningTempC) << "read " << i;
        }
    }
}

TEST(MockEnvSensor, FaultInjection) {
    MockEnvSensor sensor;
    sensor.init();
    SensorReading reading{};

    sensor.injectFailures(2);
    EXPECT_EQ(sensor.read(reading), ErrorCode::SENSOR_ERROR);
    EXPECT_EQ(sensor.read(reading), ErrorCode::SENSOR_ERROR);
    EXPECT_EQ(sensor.read(reading), ErrorCode::OK);

    sensor.setDisconnected(true);
    EXPECT_EQ(sensor.read(reading), ErrorCode::SENSOR_ERROR);
    sensor.setDisconnected(false);
    EXPECT_EQ(sensor.read(reading), ErrorCode::OK);

    sensor.injectInvalidValue();
    EXPECT_EQ(sensor.read(reading), ErrorCode::OK);
    EXPECT_TRUE(std::isnan(reading.temperatureC));
}

TEST(MockBatteryGauge, DischargesAndIsReplaced) {
    MockBatteryGauge battery(MockBatteryGauge::Config{10.0f, 4.0f, 3.0f});
    battery.init();
    float level = 0.0f;
    battery.readPercent(level);
    EXPECT_FLOAT_EQ(level, 10.0f);
    battery.readPercent(level);
    EXPECT_FLOAT_EQ(level, 6.0f);
    battery.readPercent(level);
    EXPECT_FLOAT_EQ(level, 10.0f) << "replaced once below 3 %";
}
