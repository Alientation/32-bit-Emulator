#include <gtest/gtest.h>

#include <AlienCPUTest.h>

class ValidInstructionTest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};


TEST_F(ValidInstructionTest, ValidInstructionTest_Valid) {
    // all instructions except for the first one should be valid
    for (u16 instruction = 1; instruction < cpu.INSTRUCTION_COUNT; instruction++) {
        ASSERT_TRUE(cpu.isValidInstruction(instruction));
    }
}

TEST_F(ValidInstructionTest, ValidInstructionTest_Invalid) {
    // the first instruction should be invalid
    ASSERT_FALSE(cpu.isValidInstruction(0));

    // check out of bounds instructions
    ASSERT_FALSE(cpu.isValidInstruction(-1));
    ASSERT_FALSE(cpu.isValidInstruction(cpu.INSTRUCTION_COUNT));
}