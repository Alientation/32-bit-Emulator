#include <gtest/gtest.h>
#include <AlienCPUTest.h>

class StackTest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};

TEST_F(StackTest, StackTest_SPToAddress) {
    ASSERT_EQ(cpu.SPToAddress(), 0x0001FFFF);
    cpu.SP = 0x1234;
    ASSERT_EQ(cpu.SPToAddress(), 0x00011234);
    cpu.SP = 0x0000;
    ASSERT_EQ(cpu.SPToAddress(), 0x00010000);
}

TEST_F(StackTest, StackTest_PushPCToStack) {
    cpu.PC = 0x00012345;
    cpu.pushPCToStack();
    ASSERT_EQ(cpu.SP, 0x0000FFFB);
    ASSERT_EQ(cpu.motherboard.ram.data[0x0001FFFC], 0x45);
}