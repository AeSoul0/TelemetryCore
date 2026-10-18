#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include "frame.h"

TEST(Frame, ChecksumIsXorOfPayload) {
    EXPECT_EQ(frame::checksum("A", 1), 0x41);
    EXPECT_EQ(frame::checksum("AB", 2), 0x41 ^ 0x42);
    EXPECT_EQ(frame::checksum("", 0), 0x00);
}

TEST(Frame, EncodeProducesDollarPayloadStarChecksumCrLf) {
    char out[32];
    const size_t length = frame::encode("A", out, sizeof(out));
    EXPECT_EQ(std::string(out), "$A*41\r\n");
    EXPECT_EQ(length, std::strlen(out));
}

TEST(Frame, EncodeUsesUppercaseHex) {
    char out[32];
    // 'j' ^ 'p' = 0x6A ^ 0x70 = 0x1A
    frame::encode("jp", out, sizeof(out));
    EXPECT_EQ(std::string(out), "$jp*1A\r\n");
}

TEST(Frame, EncodeReplacesReservedCharacters) {
    char out[32];
    frame::encode("a*b$c\nd", out, sizeof(out));
    char payload[32];
    ASSERT_TRUE(frame::decode(out, payload, sizeof(payload)));
    EXPECT_STREQ(payload, "a_b_c_d");
}

TEST(Frame, EncodeFailsWhenBufferTooSmall) {
    char out[7]; // "$A*41\r\n" needs 7 + NUL
    EXPECT_EQ(frame::encode("A", out, sizeof(out)), 0U);
    char exact[8];
    EXPECT_EQ(frame::encode("A", exact, sizeof(exact)), 7U);
}

TEST(Frame, DecodeRoundTrip) {
    char encoded[frame::kMaxFrameLength];
    frame::encode("TLM,1,100,23.4,1013.2,95,OK,00", encoded, sizeof(encoded));
    char payload[frame::kMaxPayloadLength];
    ASSERT_TRUE(frame::decode(encoded, payload, sizeof(payload)));
    EXPECT_STREQ(payload, "TLM,1,100,23.4,1013.2,95,OK,00");
}

TEST(Frame, DecodeAcceptsMissingLineEndingAndLowercaseHex) {
    char payload[16];
    EXPECT_TRUE(frame::decode("$jp*1a", payload, sizeof(payload)));
    EXPECT_STREQ(payload, "jp");
}

TEST(Frame, DecodeRejectsCorruptedFrames) {
    char payload[32];
    EXPECT_FALSE(frame::decode("$A*42\r\n", payload, sizeof(payload))) << "wrong checksum";
    EXPECT_FALSE(frame::decode("A*41\r\n", payload, sizeof(payload))) << "missing $";
    EXPECT_FALSE(frame::decode("$A41\r\n", payload, sizeof(payload))) << "missing *";
    EXPECT_FALSE(frame::decode("$A*4G\r\n", payload, sizeof(payload))) << "bad hex";
    EXPECT_FALSE(frame::decode("", payload, sizeof(payload))) << "empty";
    EXPECT_FALSE(frame::decode("$B*41", payload, sizeof(payload))) << "flipped payload bit";
}

TEST(Frame, DecodeRejectsUndersizedOutput) {
    char payload[2];
    EXPECT_FALSE(frame::decode("$AB*03", payload, sizeof(payload)));
}
