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

    cpu.start(2);

    EXPECT_EQ(cpu.motherboard.ram[0x0001FFFE], 0x34);
    EXPECT_EQ(cpu.motherboard.ram[0x0001FFFF], 0x12);
    EXPECT_EQ(cpu.SP, 0xFFFD);
    EXPECT_EQ(cpu.cycles, 2);
    EXPECT_EQ(cpu.PC, 0x00001024);
}
