#include <gtest/gtest.h>

#include <AlienCPUTest.h>

class INCTest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};

TEST_F(INCTest, IncrementAbsolute_Normal) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_INC_ABS);

    cpu.writeWord(0x00001024, 0x00012345);
    cpu.writeByte(0x00012345, 0x12);

    cpu.start(8);

    EXPECT_EQ(cpu.motherboard.ram[0x00012345], 0x13); // check incremented memory value
    EXPECT_EQ(cpu.PC, 0x00001028); // check PC points to next instruction
    EXPECT_EQ(cpu.cycles, 8);
    EXPECT_EQ(cpu.P, 0b00100000); // only default flag is set
}

TEST_F(INCTest, IncrementAbsolute_ZEROFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_INC_ABS);

    cpu.writeWord(0x00001024, 0x00012345);
    cpu.writeByte(0x00012345, 0xFF);

    cpu.start(8);

    EXPECT_EQ(cpu.motherboard.ram[0x00012345], 0x00); // check incremented memory value
    EXPECT_EQ(cpu.PC, 0x00001028); // check PC points to next instruction
    EXPECT_EQ(cpu.cycles, 8);
    EXPECT_EQ(cpu.P, 0b00100010); // only default flag is set
}