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
    ASSERT_FALSE(cpu.getFlag(cpu.C_FLAG));
    ASSERT_FALSE(cpu.getFlag(cpu.Z_FLAG));
    ASSERT_FALSE(cpu.getFlag(cpu.I_FLAG));
    ASSERT_FALSE(cpu.getFlag(cpu.D_FLAG));
    ASSERT_FALSE(cpu.getFlag(cpu.B_FLAG));
    ASSERT_TRUE(cpu.getFlag(cpu.UNUSED_FLAG));
    ASSERT_FALSE(cpu.getFlag(cpu.V_FLAG));
    ASSERT_FALSE(cpu.getFlag(cpu.N_FLAG));
}

TEST_F(StatusFlagTest, StatusFlagTest_GetFlag) {
    cpu.P = 0b01010101;

    ASSERT_TRUE(cpu.getFlag(cpu.C_FLAG));
    ASSERT_FALSE(cpu.getFlag(cpu.Z_FLAG));
    ASSERT_TRUE(cpu.getFlag(cpu.I_FLAG));
    ASSERT_FALSE(cpu.getFlag(cpu.D_FLAG));
    ASSERT_TRUE(cpu.getFlag(cpu.B_FLAG));
    ASSERT_FALSE(cpu.getFlag(cpu.UNUSED_FLAG));
    ASSERT_TRUE(cpu.getFlag(cpu.V_FLAG));
    ASSERT_FALSE(cpu.getFlag(cpu.N_FLAG));

    cpu.P = 0b10101010;
    ASSERT_FALSE(cpu.getFlag(cpu.C_FLAG));
    ASSERT_TRUE(cpu.getFlag(cpu.Z_FLAG));
    ASSERT_FALSE(cpu.getFlag(cpu.I_FLAG));
    ASSERT_TRUE(cpu.getFlag(cpu.D_FLAG));
    ASSERT_FALSE(cpu.getFlag(cpu.B_FLAG));
    ASSERT_TRUE(cpu.getFlag(cpu.UNUSED_FLAG));
    ASSERT_FALSE(cpu.getFlag(cpu.V_FLAG));
    ASSERT_TRUE(cpu.getFlag(cpu.N_FLAG));
}

TEST_F(StatusFlagTest, StatusFlagTest_SetFlag_TrueToTrue) {
    cpu.P = 0b11111111;

    cpu.setFlag(cpu.C_FLAG, true);
    ASSERT_EQ(cpu.P, 0b11111111);

    cpu.setFlag(cpu.Z_FLAG, true);
    ASSERT_EQ(cpu.P, 0b11111111);

    cpu.setFlag(cpu.I_FLAG, true);
    ASSERT_EQ(cpu.P, 0b11111111);

    cpu.setFlag(cpu.D_FLAG, true);
    ASSERT_EQ(cpu.P, 0b11111111);

    cpu.setFlag(cpu.B_FLAG, true);
    ASSERT_EQ(cpu.P, 0b11111111);

    cpu.setFlag(cpu.UNUSED_FLAG, true);
    ASSERT_EQ(cpu.P, 0b11111111);

    cpu.setFlag(cpu.V_FLAG, true);
    ASSERT_EQ(cpu.P, 0b11111111);

    cpu.setFlag(cpu.N_FLAG, true);
    ASSERT_EQ(cpu.P, 0b11111111);
}

TEST_F(StatusFlagTest, StatusFlagTest_SetFlag_FalseToTrue) {
    cpu.P = 0b00000000;

    cpu.setFlag(cpu.C_FLAG, true);
    ASSERT_EQ(cpu.P, 0b00000001);

    cpu.setFlag(cpu.Z_FLAG, true);
    ASSERT_EQ(cpu.P, 0b00000011);

    cpu.setFlag(cpu.I_FLAG, true);
    ASSERT_EQ(cpu.P, 0b00000111);

    cpu.setFlag(cpu.D_FLAG, true);
    ASSERT_EQ(cpu.P, 0b00001111);

    cpu.setFlag(cpu.B_FLAG, true);
    ASSERT_EQ(cpu.P, 0b00011111);

    cpu.setFlag(cpu.UNUSED_FLAG, true);
    ASSERT_EQ(cpu.P, 0b00111111);

    cpu.setFlag(cpu.V_FLAG, true);
    ASSERT_EQ(cpu.P, 0b01111111);

    cpu.setFlag(cpu.N_FLAG, true);
    ASSERT_EQ(cpu.P, 0b11111111);
}

TEST_F(StatusFlagTest, StatusFlagTest_SetFlag_TrueToFalse) {
    cpu.P = 0b11111111;

    cpu.setFlag(cpu.C_FLAG, false);
    ASSERT_EQ(cpu.P, 0b11111110);

    cpu.setFlag(cpu.Z_FLAG, false);
    ASSERT_EQ(cpu.P, 0b11111100);

    cpu.setFlag(cpu.I_FLAG, false);
    ASSERT_EQ(cpu.P, 0b11111000);

    cpu.setFlag(cpu.D_FLAG, false);
    ASSERT_EQ(cpu.P, 0b11110000);

    cpu.setFlag(cpu.B_FLAG, false);
    ASSERT_EQ(cpu.P, 0b11100000);

    cpu.setFlag(cpu.UNUSED_FLAG, false);
    ASSERT_EQ(cpu.P, 0b11000000);

    cpu.setFlag(cpu.V_FLAG, false);
    ASSERT_EQ(cpu.P, 0b10000000);

    cpu.setFlag(cpu.N_FLAG, false);
    ASSERT_EQ(cpu.P, 0b00000000);
}

TEST_F(StatusFlagTest, StatusFlagTest_SetFlag_FalseToFalse) {
    cpu.P = 0b00000000;

    cpu.setFlag(cpu.C_FLAG, false);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.setFlag(cpu.Z_FLAG, false);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.setFlag(cpu.I_FLAG, false);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.setFlag(cpu.D_FLAG, false);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.setFlag(cpu.B_FLAG, false);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.setFlag(cpu.UNUSED_FLAG, false);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.setFlag(cpu.V_FLAG, false);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.setFlag(cpu.N_FLAG, false);
    ASSERT_EQ(cpu.P, 0b00000000);
}

TEST_F(StatusFlagTest, StatusFlagTest_ClearFlag_TrueToFalse) {
    cpu.P = 0b11111111;

    cpu.clearFlag(cpu.C_FLAG);
    ASSERT_EQ(cpu.P, 0b11111110);

    cpu.clearFlag(cpu.Z_FLAG);
    ASSERT_EQ(cpu.P, 0b11111100);

    cpu.clearFlag(cpu.I_FLAG);
    ASSERT_EQ(cpu.P, 0b11111000);

    cpu.clearFlag(cpu.D_FLAG);
    ASSERT_EQ(cpu.P, 0b11110000);

    cpu.clearFlag(cpu.B_FLAG);
    ASSERT_EQ(cpu.P, 0b11100000);

    cpu.clearFlag(cpu.UNUSED_FLAG);
    ASSERT_EQ(cpu.P, 0b11000000);

    cpu.clearFlag(cpu.V_FLAG);
    ASSERT_EQ(cpu.P, 0b10000000);

    cpu.clearFlag(cpu.N_FLAG);
    ASSERT_EQ(cpu.P, 0b00000000);
}

TEST_F(StatusFlagTest, StatusFlagTest_ClearFlag_FalseToFalse) {
    cpu.P = 0b00000000;

    cpu.clearFlag(cpu.C_FLAG);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.clearFlag(cpu.Z_FLAG);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.clearFlag(cpu.I_FLAG);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.clearFlag(cpu.D_FLAG);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.clearFlag(cpu.B_FLAG);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.clearFlag(cpu.UNUSED_FLAG);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.clearFlag(cpu.V_FLAG);
    ASSERT_EQ(cpu.P, 0b00000000);

    cpu.clearFlag(cpu.N_FLAG);
    ASSERT_EQ(cpu.P, 0b00000000);
}