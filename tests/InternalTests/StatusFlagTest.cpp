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
    ASSERT_FALSE(cpu.getFlag(CARRY));
    ASSERT_FALSE(cpu.getFlag(ZERO));
    ASSERT_FALSE(cpu.getFlag(INTERRUPT));
    ASSERT_FALSE(cpu.getFlag(DECIMAL));
    ASSERT_FALSE(cpu.getFlag(BREAK));
    ASSERT_TRUE(cpu.getFlag(UNUSED));
    ASSERT_FALSE(cpu.getFlag(OVERFLOW));
    ASSERT_FALSE(cpu.getFlag(NEGATIVE));
}

TEST_F(StatusFlagTest, StatusFlagTest_GetFlag) {
    cpu.P = 0b01010101;

    ASSERT_TRUE(cpu.getFlag(CARRY));
    ASSERT_FALSE(cpu.getFlag(ZERO));
    ASSERT_TRUE(cpu.getFlag(INTERRUPT));
    ASSERT_FALSE(cpu.getFlag(DECIMAL));
    ASSERT_TRUE(cpu.getFlag(BREAK));
    ASSERT_FALSE(cpu.getFlag(UNUSED));
    ASSERT_TRUE(cpu.getFlag(OVERFLOW));
    ASSERT_FALSE(cpu.getFlag(NEGATIVE));

    cpu.P = 0b10101010;
    ASSERT_FALSE(cpu.getFlag(CARRY));
    ASSERT_TRUE(cpu.getFlag(ZERO));
    ASSERT_FALSE(cpu.getFlag(INTERRUPT));
    ASSERT_TRUE(cpu.getFlag(DECIMAL));
    ASSERT_FALSE(cpu.getFlag(BREAK));
    ASSERT_TRUE(cpu.getFlag(UNUSED));
    ASSERT_FALSE(cpu.getFlag(OVERFLOW));
    ASSERT_TRUE(cpu.getFlag(NEGATIVE));
}

TEST_F(StatusFlagTest, StatusFlagTest_SetFlag_TrueToTrue) {
    cpu.P = 0b11111111;

    cpu.setFlag(CARRY, true);
    ASSERT_EQ(cpu.P, 0b11111111);

    cpu.setFlag(ZERO, true);
    ASSERT_EQ(cpu.P, 0b11111111);

    cpu.setFlag(INTERRUPT, true);
    ASSERT_EQ(cpu.P, 0b11111111);

    cpu.setFlag(DECIMAL, true);
    ASSERT_EQ(cpu.P, 0b11111111);

    cpu.setFlag(BREAK, true);
    ASSERT_EQ(cpu.P, 0b11111111);

    cpu.setFlag(UNUSED, true);
    ASSERT_EQ(cpu.P, 0b11111111);

    cpu.setFlag(OVERFLOW, true);
    ASSERT_EQ(cpu.P, 0b11111111);

    cpu.setFlag(NEGATIVE, true);
    ASSERT_EQ(cpu.P, 0b11111111);
}

TEST_F(StatusFlagTest, StatusFlagTest_SetFlag_FalseToTrue) {
    cpu.P = 0b00000000;

    cpu.setFlag(CARRY, true);
    ASSERT_EQ(cpu.P, 0b00000001);

    cpu.setFlag(ZERO, true);
    ASSERT_EQ(cpu.P, 0b00000011);

    cpu.setFlag(INTERRUPT, true);
    ASSERT_EQ(cpu.P, 0b00000111);

    cpu.setFlag(DECIMAL, true);
    ASSERT_EQ(cpu.P, 0b00001111);

    cpu.setFlag(BREAK, true);
    ASSERT_EQ(cpu.P, 0b00011111);

    cpu.setFlag(UNUSED, true);
    ASSERT_EQ(cpu.P, 0b00111111);

    cpu.setFlag(OVERFLOW, true);
    ASSERT_EQ(cpu.P, 0b01111111);

    cpu.setFlag(NEGATIVE, true);
    ASSERT_EQ(cpu.P, 0b11111111);
}

TEST_F(StatusFlagTest, StatusFlagTest_SetFlag_TrueToFalse) {
    cpu.P = 0b11111111;

    cpu.setFlag(CARRY, false);
    ASSERT_EQ(cpu.P, 0b11111110);

    cpu.setFlag(ZERO, false);
    ASSERT_EQ(cpu.P, 0b11111100);

    cpu.setFlag(INTERRUPT, false);
    ASSERT_EQ(cpu.P, 0b11111000);

    cpu.setFlag(DECIMAL, false);
    ASSERT_EQ(cpu.P, 0b11110000);

    cpu.setFlag(BREAK, false);
    ASSERT_EQ(cpu.P, 0b11100000);

    cpu.setFlag(UNUSED, false);
    ASSERT_EQ(cpu.P, 0b11000000);

    cpu.setFlag(OVERFLOW, false);
    ASSERT_EQ(cpu.P, 0b10000000);

    cpu.setFlag(NEGATIVE, false);
    ASSERT_EQ(cpu.P, 0b00000000);
}

TEST_F(StatusFlagTest, StatusFlagTest_SetFlag_FalseToFalse) {
    cpu.P = 0b00000000;

    cpu.setFlag(CARRY, false);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.setFlag(ZERO, false);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.setFlag(INTERRUPT, false);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.setFlag(DECIMAL, false);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.setFlag(BREAK, false);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.setFlag(UNUSED, false);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.setFlag(OVERFLOW, false);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.setFlag(NEGATIVE, false);
    ASSERT_EQ(cpu.P, 0b00000000);
}

TEST_F(StatusFlagTest, StatusFlagTest_ClearFlag_TrueToFalse) {
    cpu.P = 0b11111111;

    cpu.clearFlag(CARRY);
    ASSERT_EQ(cpu.P, 0b11111110);

    cpu.clearFlag(ZERO);
    ASSERT_EQ(cpu.P, 0b11111100);

    cpu.clearFlag(INTERRUPT);
    ASSERT_EQ(cpu.P, 0b11111000);

    cpu.clearFlag(DECIMAL);
    ASSERT_EQ(cpu.P, 0b11110000);

    cpu.clearFlag(BREAK);
    ASSERT_EQ(cpu.P, 0b11100000);

    cpu.clearFlag(UNUSED);
    ASSERT_EQ(cpu.P, 0b11000000);

    cpu.clearFlag(OVERFLOW);
    ASSERT_EQ(cpu.P, 0b10000000);

    cpu.clearFlag(NEGATIVE);
    ASSERT_EQ(cpu.P, 0b00000000);
}

TEST_F(StatusFlagTest, StatusFlagTest_ClearFlag_FalseToFalse) {
    cpu.P = 0b00000000;

    cpu.clearFlag(CARRY);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.clearFlag(ZERO);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.clearFlag(INTERRUPT);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.clearFlag(DECIMAL);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.clearFlag(BREAK);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.clearFlag(UNUSED);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.clearFlag(OVERFLOW);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.clearFlag(NEGATIVE);
    ASSERT_EQ(cpu.P, 0b00000000);
}