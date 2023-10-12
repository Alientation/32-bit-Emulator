#include <gtest/gtest.h>

#include <AlienCPUTest.h>

class PLATest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};

TEST_F(PLATest, PopAccumulatorImplied_Normal) {
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