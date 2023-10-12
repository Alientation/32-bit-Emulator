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


TEST_F(StackTest, PushAccumulatorImplied_Normal) {
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_PHA_IMPL);

    cpu.A = 0x1234;

    cpu.start(3);

    EXPECT_EQ(cpu.motherboard.ram[0x0001FFFE], 0x34);
    EXPECT_EQ(cpu.motherboard.ram[0x0001FFFF], 0x12);
    EXPECT_EQ(cpu.SP, 0xFFFD);
    EXPECT_EQ(cpu.cycles, 3);
    EXPECT_EQ(cpu.PC, 0x00001024);
}

TEST_F(StackTest, PushProcessorImplied_Normal) {
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_PHP_IMPL);

    cpu.P = 0b01010101;

    cpu.start(2);

    EXPECT_EQ(cpu.motherboard.ram[0x0001FFFF], 0b01010101);
    EXPECT_EQ(cpu.SP, 0xFFFE);
    EXPECT_EQ(cpu.cycles, 2);
    EXPECT_EQ(cpu.PC, 0x00001024);
}

TEST_F(StackTest, PopAccumulatorImplied_Normal) {
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_PLA_IMPL);

    cpu.motherboard.ram[0x0001FFFE] = 0x34;
    cpu.motherboard.ram[0x0001FFFF] = 0x12;
    cpu.SP = 0xFFFD;

    cpu.start(3);

    EXPECT_EQ(cpu.A, 0x1234);
    EXPECT_EQ(cpu.SP, 0xFFFF);
    EXPECT_EQ(cpu.cycles, 3);
    EXPECT_EQ(cpu.PC, 0x00001024);
}

TEST_F(StackTest, PopProcessorImplied_Normal) {
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_PLP_IMPL);

    cpu.motherboard.ram[0x0001FFFF] = 0b01010101;
    cpu.SP = 0xFFFE;

    cpu.start(2);

    EXPECT_EQ(cpu.P, 0b01010101);
    EXPECT_EQ(cpu.SP, 0xFFFF);
    EXPECT_EQ(cpu.cycles, 2);
    EXPECT_EQ(cpu.PC, 0x00001024);
}