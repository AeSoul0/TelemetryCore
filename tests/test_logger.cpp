#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "logger.h"

namespace {

std::vector<std::string> captured;
void captureSink(const char* payload) {
    captured.emplace_back(payload);
}

class LoggerTest : public ::testing::Test {
  protected:
    void SetUp() override {
        captured.clear();
        logger::setSink(captureSink);
        logger::setMinLevel(logger::Level::Debug);
    }
    void TearDown() override {
        logger::setSink(nullptr);
        logger::setMinLevel(logger::Level::Debug);
    }
};

} // namespace

TEST_F(LoggerTest, FormatsStructuredPayload) {
    LOG_WARN("SensorTask", "retry %d/%d", 2, 3);
    ASSERT_EQ(captured.size(), 1U);
    EXPECT_EQ(captured[0], "LOG,WARN,SensorTask,retry 2/3");
}

TEST_F(LoggerTest, AllLevels) {
    LOG_DEBUG("C", "d");
    LOG_INFO("C", "i");
    LOG_WARN("C", "w");
    LOG_ERROR("C", "e");
    ASSERT_EQ(captured.size(), 4U);
    EXPECT_EQ(captured[0], "LOG,DEBUG,C,d");
    EXPECT_EQ(captured[1], "LOG,INFO,C,i");
    EXPECT_EQ(captured[2], "LOG,WARN,C,w");
    EXPECT_EQ(captured[3], "LOG,ERROR,C,e");
}

TEST_F(LoggerTest, RuntimeLevelFiltersVerboseLogs) {
    logger::setMinLevel(logger::Level::Warn);
    LOG_DEBUG("C", "hidden");
    LOG_INFO("C", "hidden");
    LOG_WARN("C", "shown");
    ASSERT_EQ(captured.size(), 1U);
    EXPECT_EQ(captured[0], "LOG,WARN,C,shown");
}

TEST_F(LoggerTest, LongMessagesAreTruncated) {
    const std::string longText(500, 'x');
    LOG_INFO("C", "%s", longText.c_str());
    ASSERT_EQ(captured.size(), 1U);
    EXPECT_EQ(captured[0].size(), logger::kMaxPayloadLength - 1);
    EXPECT_EQ(captured[0].rfind("LOG,INFO,C,x", 0), 0U);
}

TEST_F(LoggerTest, NoSinkMeansNoOutput) {
    logger::setSink(nullptr);
    LOG_ERROR("C", "dropped");
    EXPECT_TRUE(captured.empty());
}
