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
    ASSERT_EQ(cpu.motherboard.ram.data[0x0001FFFD], 0x23);
    ASSERT_EQ(cpu.motherboard.ram.data[0x0001FFFE], 0x01);
    ASSERT_EQ(cpu.motherboard.ram.data[0x0001FFFF], 0x00);
}

TEST_F(StackTest, StackTest_PopPCFromStack) {
    cpu.SP = 0xFFFB;
    cpu.motherboard.ram.data[0x0001FFFC] = 0x45;
    cpu.motherboard.ram.data[0x0001FFFD] = 0x23;
    cpu.motherboard.ram.data[0x0001FFFE] = 0x01;
    cpu.motherboard.ram.data[0x0001FFFF] = 0x00;
    cpu.popPCFromStack();
    ASSERT_EQ(cpu.SP, 0xFFFF);
    ASSERT_EQ(cpu.PC, 0x00012345);
}

TEST_F(StackTest, StackTest_PushWordToStack) {
    cpu.pushWordToStack(0x12345678);
    ASSERT_EQ(cpu.SP, 0x0000FFFB);
    ASSERT_EQ(cpu.motherboard.ram.data[0x0001FFFC], 0x78);
    ASSERT_EQ(cpu.motherboard.ram.data[0x0001FFFD], 0x56);
    ASSERT_EQ(cpu.motherboard.ram.data[0x0001FFFE], 0x34);
    ASSERT_EQ(cpu.motherboard.ram.data[0x0001FFFF], 0x12);
}

TEST_F(StackTest, StackTest_PopWordFromStack) {
    cpu.SP = 0xFFFB;
    cpu.motherboard.ram.data[0x0001FFFC] = 0x78;
    cpu.motherboard.ram.data[0x0001FFFD] = 0x56;
    cpu.motherboard.ram.data[0x0001FFFE] = 0x34;
    cpu.motherboard.ram.data[0x0001FFFF] = 0x12;
    ASSERT_EQ(cpu.popWordFromStack(), 0x12345678);
    ASSERT_EQ(cpu.SP, 0xFFFF);
}

TEST_F(StackTest, StackTest_PushTwoBytesToStack) {
    cpu.pushTwoBytesToStack(0x1234);
    ASSERT_EQ(cpu.SP, 0x0000FFFD);
    ASSERT_EQ(cpu.motherboard.ram.data[0x0001FFFE], 0x34);
    ASSERT_EQ(cpu.motherboard.ram.data[0x0001FFFF], 0x12);
}

TEST_F(StackTest, StackTest_PopTwoBytesFromStack) {
    cpu.SP = 0xFFFD;
    cpu.motherboard.ram.data[0x0001FFFE] = 0x34;
    cpu.motherboard.ram.data[0x0001FFFF] = 0x12;
    ASSERT_EQ(cpu.popTwoBytesFromStack(), 0x1234);
    ASSERT_EQ(cpu.SP, 0xFFFF);
}

TEST_F(StackTest, StackTest_PushByteToStack) {
    cpu.pushByteToStack(0x12);
    ASSERT_EQ(cpu.SP, 0x0000FFFE);
    ASSERT_EQ(cpu.motherboard.ram.data[0x0001FFFF], 0x12);
}

TEST_F(StackTest, StackTest_PopByteFromStack) {
    cpu.SP = 0xFFFE;
    cpu.motherboard.ram.data[0x0001FFFF] = 0x12;
    ASSERT_EQ(cpu.popByteFromStack(), 0x12);
    ASSERT_EQ(cpu.SP, 0xFFFF);
}