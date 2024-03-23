#include <gtest/gtest.h>
#include <emulator6502_tests\AlienCPUTest.h>

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
    // all instructions should be valid
    for (u16 instruction = 0; instruction < cpu.INSTRUCTION_COUNT; instruction++) {
        ASSERT_TRUE(cpu.isValidInstruction(instruction));
    }
}

TEST_F(ValidInstructionTest, ValidInstructionTest_Invalid) {
    // check out of bounds instructions
    ASSERT_FALSE(cpu.isValidInstruction(-1));
    ASSERT_FALSE(cpu.isValidInstruction(cpu.INSTRUCTION_COUNT));
}