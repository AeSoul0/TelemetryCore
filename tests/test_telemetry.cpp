#include <gtest/gtest.h>

#include <cstring>
#include <limits>
#include <string>

#include "telemetry.h"

namespace {

SensorSample validSample(float temperatureC, float pressureHpa = 1013.2f, float batteryPct = 90.0f) {
    SensorSample sample{};
    sample.reading.timestampMs = 1200;
    sample.reading.temperatureC = temperatureC;
    sample.reading.pressureHpa = pressureHpa;
    sample.batteryPct = batteryPct;
    sample.error = ErrorCode::OK;
    sample.attempts = 1;
    return sample;
}

SensorSample failedSample() {
    SensorSample sample = validSample(0.0f);
    sample.error = ErrorCode::SENSOR_ERROR;
    sample.attempts = Acquisition::kDefaultMaxAttempts;
    return sample;
}

std::string serialize(const TelemetryPacket& packet) {
    char buffer[96];
    const size_t length = TelemetryProcessor::serialize(packet, buffer, sizeof(buffer));
    return std::string(buffer, length);
}

} // namespace

// --- Rules (roadmap phase 6.2 / 6.4) -------------------------------------------------------------

TEST(TelemetryRules, TemperatureThresholds) {
    const TelemetryProcessor processor;
    EXPECT_EQ(processor.calculateStatus(25.0f), SystemStatus::OK);
    EXPECT_EQ(processor.calculateStatus(55.0f), SystemStatus::WARNING);
    EXPECT_EQ(processor.calculateStatus(85.0f), SystemStatus::CRITICAL);
}

TEST(TelemetryRules, ThresholdBoundaries) {
    const TelemetryProcessor processor;
    EXPECT_EQ(processor.calculateStatus(49.99f), SystemStatus::OK);
    EXPECT_EQ(processor.calculateStatus(50.0f), SystemStatus::WARNING);
    EXPECT_EQ(processor.calculateStatus(79.99f), SystemStatus::WARNING);
    EXPECT_EQ(processor.calculateStatus(80.0f), SystemStatus::CRITICAL);
}

TEST(TelemetryRules, EdgeCaseTemperatures) {
    const TelemetryProcessor processor;
    EXPECT_EQ(processor.calculateStatus(0.0f), SystemStatus::OK);
    EXPECT_EQ(processor.calculateStatus(-20.0f), SystemStatus::OK);
    EXPECT_EQ(processor.calculateStatus(100.0f), SystemStatus::CRITICAL);
    EXPECT_EQ(processor.calculateStatus(std::numeric_limits<float>::quiet_NaN()), SystemStatus::CRITICAL);
    EXPECT_EQ(processor.calculateStatus(std::numeric_limits<float>::infinity()), SystemStatus::CRITICAL);
}

TEST(TelemetryRules, BatteryThresholds) {
    const TelemetryProcessor processor;
    EXPECT_EQ(processor.batteryStatus(50.0f), SystemStatus::OK);
    EXPECT_EQ(processor.batteryStatus(20.0f), SystemStatus::OK);
    EXPECT_EQ(processor.batteryStatus(19.9f), SystemStatus::WARNING);
    EXPECT_EQ(processor.batteryStatus(4.9f), SystemStatus::CRITICAL);
}

TEST(TelemetryRules, Plausibility) {
    const TelemetryProcessor processor;
    EXPECT_TRUE(processor.isPlausible(validSample(23.4f)));
    EXPECT_TRUE(processor.isPlausible(validSample(-40.0f)));
    EXPECT_FALSE(processor.isPlausible(validSample(-41.0f)));
    EXPECT_FALSE(processor.isPlausible(validSample(126.0f)));
    EXPECT_FALSE(processor.isPlausible(validSample(std::numeric_limits<float>::quiet_NaN())));
    EXPECT_FALSE(processor.isPlausible(validSample(-std::numeric_limits<float>::infinity())));
    EXPECT_FALSE(processor.isPlausible(validSample(23.0f, 200.0f)));
    EXPECT_FALSE(processor.isPlausible(validSample(23.0f, 1013.0f, 101.0f)));
}

// --- Processing ----------------------------------------------------------------------------------

TEST(TelemetryProcessing, ValidSampleProducesPacket) {
    TelemetryProcessor processor;
    const TelemetryPacket packet = processor.process(validSample(55.0f));
    EXPECT_EQ(packet.timestampMs, 1200U);
    EXPECT_FLOAT_EQ(packet.temperatureC, 55.0f);
    EXPECT_FLOAT_EQ(packet.pressureHpa, 1013.2f);
    EXPECT_FLOAT_EQ(packet.batteryPct, 90.0f);
    EXPECT_EQ(packet.status, SystemStatus::WARNING);
    EXPECT_EQ(packet.faultFlags, 0U);
}

TEST(TelemetryProcessing, SequenceIncrements) {
    TelemetryProcessor processor;
    EXPECT_EQ(processor.process(validSample(20.0f)).sequence, 0U);
    EXPECT_EQ(processor.process(validSample(20.0f)).sequence, 1U);
    EXPECT_EQ(processor.process(failedSample()).sequence, 2U);
}

TEST(TelemetryProcessing, StatusIsWorstOfTemperatureAndBattery) {
    TelemetryProcessor processor;
    const TelemetryPacket lowBattery = processor.process(validSample(25.0f, 1013.0f, 10.0f));
    EXPECT_EQ(lowBattery.status, SystemStatus::WARNING);
    EXPECT_EQ(lowBattery.faultFlags, FaultFlags::kBatteryLow);

    const TelemetryPacket hotAndLow = processor.process(validSample(85.0f, 1013.0f, 10.0f));
    EXPECT_EQ(hotAndLow.status, SystemStatus::CRITICAL);
}

TEST(TelemetryProcessing, PersistentErrorIsWarningAndKeepsLastValues) {
    TelemetryProcessor processor;
    processor.process(validSample(23.4f));
    const TelemetryPacket packet = processor.process(failedSample());
    EXPECT_EQ(packet.status, SystemStatus::WARNING);
    EXPECT_EQ(packet.faultFlags, FaultFlags::kSensorError);
    EXPECT_FLOAT_EQ(packet.temperatureC, 23.4f) << "last known value is reported";
    EXPECT_EQ(processor.consecutiveFailures(), 1U);
}

TEST(TelemetryProcessing, SevereErrorEscalatesToCritical) {
    Thresholds thresholds;
    thresholds.failuresForCritical = 3;
    TelemetryProcessor processor(thresholds);
    EXPECT_EQ(processor.process(failedSample()).status, SystemStatus::WARNING);
    EXPECT_EQ(processor.process(failedSample()).status, SystemStatus::WARNING);
    EXPECT_EQ(processor.process(failedSample()).status, SystemStatus::CRITICAL);
    EXPECT_EQ(processor.process(failedSample()).status, SystemStatus::CRITICAL);
}

TEST(TelemetryProcessing, RecoversOnFirstValidSample) {
    TelemetryProcessor processor;
    for (int i = 0; i < 12; ++i) {
        processor.process(failedSample());
    }
    const TelemetryPacket packet = processor.process(validSample(24.0f));
    EXPECT_EQ(packet.status, SystemStatus::OK);
    EXPECT_EQ(packet.faultFlags, 0U);
    EXPECT_EQ(processor.consecutiveFailures(), 0U);
}

TEST(TelemetryProcessing, ImplausibleValueIsInvalidData) {
    TelemetryProcessor processor;
    const TelemetryPacket packet = processor.process(validSample(std::numeric_limits<float>::quiet_NaN()));
    EXPECT_EQ(packet.status, SystemStatus::WARNING);
    EXPECT_EQ(packet.faultFlags, FaultFlags::kInvalidData);
}

TEST(TelemetryProcessing, DroppedItemsRaiseQueueOverflow) {
    TelemetryProcessor processor;
    EXPECT_EQ(processor.process(validSample(20.0f), 2).faultFlags, FaultFlags::kQueueOverflow);
    EXPECT_EQ(processor.process(validSample(20.0f), 0).faultFlags, 0U);
}

// --- Serialization (roadmap phase 6.3) -----------------------------------------------------------

TEST(TelemetrySerialization, Format) {
    TelemetryPacket packet{42, 4200, 23.6f, 1013.25f, 87.4f, SystemStatus::OK, 0};
    EXPECT_EQ(serialize(packet), "TLM,42,4200,23.6,1013.3,87,OK,00");
}

TEST(TelemetrySerialization, StatusAndFlags) {
    TelemetryPacket packet{
        1, 100, 82.5f, 1000.0f, 3.0f, SystemStatus::CRITICAL, FaultFlags::kSensorError | FaultFlags::kBatteryLow};
    EXPECT_EQ(serialize(packet), "TLM,1,100,82.5,1000.0,3,CRITICAL,09");
}

TEST(TelemetrySerialization, NegativeAndRoundedValues) {
    TelemetryPacket packet{0, 0, -0.45f, 999.96f, 100.0f, SystemStatus::WARNING, 0};
    EXPECT_EQ(serialize(packet), "TLM,0,0,-0.5,1000.0,100,WARNING,00");
    packet.temperatureC = -12.34f;
    EXPECT_EQ(serialize(packet), "TLM,0,0,-12.3,1000.0,100,WARNING,00");
}

TEST(TelemetrySerialization, NonFiniteValuesNeverReachTheWire) {
    TelemetryPacket packet{0,
                           0,
                           std::numeric_limits<float>::quiet_NaN(),
                           std::numeric_limits<float>::infinity(),
                           std::numeric_limits<float>::quiet_NaN(),
                           SystemStatus::CRITICAL,
                           0};
    EXPECT_EQ(serialize(packet), "TLM,0,0,0.0,0.0,0,CRITICAL,00");
}

TEST(TelemetrySerialization, BufferTooSmallReturnsZero) {
    TelemetryPacket packet{42, 4200, 23.6f, 1013.25f, 87.0f, SystemStatus::OK, 0};
    char small[10];
    EXPECT_EQ(TelemetryProcessor::serialize(packet, small, sizeof(small)), 0U);
    EXPECT_STREQ(small, "");
    EXPECT_EQ(TelemetryProcessor::serialize(packet, nullptr, 0), 0U);
}
