#include <gtest/gtest.h>

#include "bit_utils.h"

TEST(BitUtils, EightBitRegister) {
    uint8_t reg = 0;
    BitUtils::setBit(reg, 3);
    EXPECT_EQ(reg, 0x08);
    EXPECT_TRUE(BitUtils::readBit(reg, 3));
    BitUtils::toggleBit(reg, 0);
    EXPECT_EQ(reg, 0x09);
    BitUtils::clearBit(reg, 3);
    EXPECT_EQ(reg, 0x01);
    EXPECT_FALSE(BitUtils::readBit(reg, 3));
}

TEST(BitUtils, ThirtyTwoBitRegisterTopBit) {
    uint32_t reg = 0;
    BitUtils::setBit(reg, 31);
    EXPECT_EQ(reg, 0x80000000U);
    EXPECT_TRUE(BitUtils::readBit(reg, 31));
    BitUtils::clearBit(reg, 31);
    EXPECT_EQ(reg, 0U);
}

TEST(BitUtils, UsableInConstantExpressions) {
    static_assert(BitUtils::bitMask<uint16_t>(15) == 0x8000);
    SUCCEED();
}
