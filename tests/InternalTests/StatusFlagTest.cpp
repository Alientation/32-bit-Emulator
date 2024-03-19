#include <gtest/gtest.h>
#include <AlienCPUTest.h>

class StatusFlagTest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};


TEST_F(StatusFlagTest, StatusFlagTest_Default) {
    // all status flags should be false by default
    ASSERT_FALSE(cpu.getFlag(CARRY_FLAG));
    ASSERT_FALSE(cpu.getFlag(ZERO_FLAG));
    ASSERT_FALSE(cpu.getFlag(INTERRUPT_FLAG));
    ASSERT_FALSE(cpu.getFlag(DECIMAL_FLAG));
    ASSERT_FALSE(cpu.getFlag(BREAK_FLAG));
    ASSERT_TRUE(cpu.getFlag(UNUSED_FLAG));
    ASSERT_FALSE(cpu.getFlag(OVERFLOW_FLAG));
    ASSERT_FALSE(cpu.getFlag(NEGATIVE_FLAG));
}

TEST_F(StatusFlagTest, StatusFlagTest_GetFlag) {
    cpu.P = 0b01010101;

    ASSERT_TRUE(cpu.getFlag(CARRY_FLAG));
    ASSERT_FALSE(cpu.getFlag(ZERO_FLAG));
    ASSERT_TRUE(cpu.getFlag(INTERRUPT_FLAG));
    ASSERT_FALSE(cpu.getFlag(DECIMAL_FLAG));
    ASSERT_TRUE(cpu.getFlag(BREAK_FLAG));
    ASSERT_FALSE(cpu.getFlag(UNUSED_FLAG));
    ASSERT_TRUE(cpu.getFlag(OVERFLOW_FLAG));
    ASSERT_FALSE(cpu.getFlag(NEGATIVE_FLAG));

    cpu.P = 0b10101010;
    ASSERT_FALSE(cpu.getFlag(CARRY_FLAG));
    ASSERT_TRUE(cpu.getFlag(ZERO_FLAG));
    ASSERT_FALSE(cpu.getFlag(INTERRUPT_FLAG));
    ASSERT_TRUE(cpu.getFlag(DECIMAL_FLAG));
    ASSERT_FALSE(cpu.getFlag(BREAK_FLAG));
    ASSERT_TRUE(cpu.getFlag(UNUSED_FLAG));
    ASSERT_FALSE(cpu.getFlag(OVERFLOW_FLAG));
    ASSERT_TRUE(cpu.getFlag(NEGATIVE_FLAG));
}

TEST_F(StatusFlagTest, StatusFlagTest_SetFlag_TrueToTrue) {
    cpu.P = 0b11111111;

    cpu.setFlag(CARRY_FLAG, true);
    ASSERT_EQ(cpu.P, 0b11111111);

    cpu.setFlag(ZERO_FLAG, true);
    ASSERT_EQ(cpu.P, 0b11111111);

    cpu.setFlag(INTERRUPT_FLAG, true);
    ASSERT_EQ(cpu.P, 0b11111111);

    cpu.setFlag(DECIMAL_FLAG, true);
    ASSERT_EQ(cpu.P, 0b11111111);

    cpu.setFlag(BREAK_FLAG, true);
    ASSERT_EQ(cpu.P, 0b11111111);

    cpu.setFlag(UNUSED_FLAG, true);
    ASSERT_EQ(cpu.P, 0b11111111);

    cpu.setFlag(OVERFLOW_FLAG, true);
    ASSERT_EQ(cpu.P, 0b11111111);

    cpu.setFlag(NEGATIVE_FLAG, true);
    ASSERT_EQ(cpu.P, 0b11111111);
}

TEST_F(StatusFlagTest, StatusFlagTest_SetFlag_FalseToTrue) {
    cpu.P = 0b00000000;

    cpu.setFlag(CARRY_FLAG, true);
    ASSERT_EQ(cpu.P, 0b00000001);

    cpu.setFlag(ZERO_FLAG, true);
    ASSERT_EQ(cpu.P, 0b00000011);

    cpu.setFlag(INTERRUPT_FLAG, true);
    ASSERT_EQ(cpu.P, 0b00000111);

    cpu.setFlag(DECIMAL_FLAG, true);
    ASSERT_EQ(cpu.P, 0b00001111);

    cpu.setFlag(BREAK_FLAG, true);
    ASSERT_EQ(cpu.P, 0b00011111);

    cpu.setFlag(UNUSED_FLAG, true);
    ASSERT_EQ(cpu.P, 0b00111111);

    cpu.setFlag(OVERFLOW_FLAG, true);
    ASSERT_EQ(cpu.P, 0b01111111);

    cpu.setFlag(NEGATIVE_FLAG, true);
    ASSERT_EQ(cpu.P, 0b11111111);
}

TEST_F(StatusFlagTest, StatusFlagTest_SetFlag_TrueToFalse) {
    cpu.P = 0b11111111;

    cpu.setFlag(CARRY_FLAG, false);
    ASSERT_EQ(cpu.P, 0b11111110);

    cpu.setFlag(ZERO_FLAG, false);
    ASSERT_EQ(cpu.P, 0b11111100);

    cpu.setFlag(INTERRUPT_FLAG, false);
    ASSERT_EQ(cpu.P, 0b11111000);

    cpu.setFlag(DECIMAL_FLAG, false);
    ASSERT_EQ(cpu.P, 0b11110000);

    cpu.setFlag(BREAK_FLAG, false);
    ASSERT_EQ(cpu.P, 0b11100000);

    cpu.setFlag(UNUSED_FLAG, false);
    ASSERT_EQ(cpu.P, 0b11000000);

    cpu.setFlag(OVERFLOW_FLAG, false);
    ASSERT_EQ(cpu.P, 0b10000000);

    cpu.setFlag(NEGATIVE_FLAG, false);
    ASSERT_EQ(cpu.P, 0b00000000);
}

TEST_F(StatusFlagTest, StatusFlagTest_SetFlag_FalseToFalse) {
    cpu.P = 0b00000000;

    cpu.setFlag(CARRY_FLAG, false);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.setFlag(ZERO_FLAG, false);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.setFlag(INTERRUPT_FLAG, false);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.setFlag(DECIMAL_FLAG, false);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.setFlag(BREAK_FLAG, false);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.setFlag(UNUSED_FLAG, false);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.setFlag(OVERFLOW_FLAG, false);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.setFlag(NEGATIVE_FLAG, false);
    ASSERT_EQ(cpu.P, 0b00000000);
}

TEST_F(StatusFlagTest, StatusFlagTest_ClearFlag_TrueToFalse) {
    cpu.P = 0b11111111;

    cpu.clearFlag(CARRY_FLAG);
    ASSERT_EQ(cpu.P, 0b11111110);

    cpu.clearFlag(ZERO_FLAG);
    ASSERT_EQ(cpu.P, 0b11111100);

    cpu.clearFlag(INTERRUPT_FLAG);
    ASSERT_EQ(cpu.P, 0b11111000);

    cpu.clearFlag(DECIMAL_FLAG);
    ASSERT_EQ(cpu.P, 0b11110000);

    cpu.clearFlag(BREAK_FLAG);
    ASSERT_EQ(cpu.P, 0b11100000);

    cpu.clearFlag(UNUSED_FLAG);
    ASSERT_EQ(cpu.P, 0b11000000);

    cpu.clearFlag(OVERFLOW_FLAG);
    ASSERT_EQ(cpu.P, 0b10000000);

    cpu.clearFlag(NEGATIVE_FLAG);
    ASSERT_EQ(cpu.P, 0b00000000);
}

TEST_F(StatusFlagTest, StatusFlagTest_ClearFlag_FalseToFalse) {
    cpu.P = 0b00000000;

    cpu.clearFlag(CARRY_FLAG);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.clearFlag(ZERO_FLAG);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.clearFlag(INTERRUPT_FLAG);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.clearFlag(DECIMAL_FLAG);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.clearFlag(BREAK_FLAG);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.clearFlag(UNUSED_FLAG);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.clearFlag(OVERFLOW_FLAG);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.clearFlag(NEGATIVE_FLAG);
    ASSERT_EQ(cpu.P, 0b00000000);
}