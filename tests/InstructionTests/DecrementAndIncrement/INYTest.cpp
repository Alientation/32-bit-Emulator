#include <AlienCPUTest.h>

class INYTest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};

TEST_F(INYTest, IncrementYImplied_Normal) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_INY_IMPL);
    cpu.Y = 0x1234;

    cpu.start(2);

    EXPECT_EQ(cpu.Y, 0x1235); // check incremented memory value
    EXPECT_EQ(cpu.PC, 0x00001024); // check PC points to next instruction
    EXPECT_EQ(cpu.cycles, 2);
    EXPECT_EQ(cpu.P, 0b00100000); // only default flag is set
}

TEST_F(INYTest, IncrementYImplied_ZEROFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_INY_IMPL);
    cpu.Y = 0xFFFF;

    cpu.start(2);

    EXPECT_EQ(cpu.Y, 0x0000); // check incremented memory value
    EXPECT_EQ(cpu.PC, 0x00001024); // check PC points to next instruction
    EXPECT_EQ(cpu.cycles, 2);
    EXPECT_EQ(cpu.P, 0b00100010); // only default flag is set
}

TEST_F(INYTest, IncrementYImplied_NEGATIVEFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_INY_IMPL);
    cpu.Y = 0xFFFE;

    cpu.start(2);

    EXPECT_EQ(cpu.Y, 0xFFFF); // check incremented memory value
    EXPECT_EQ(cpu.PC, 0x00001024); // check PC points to next instruction
    EXPECT_EQ(cpu.cycles, 2);
    EXPECT_EQ(cpu.P, 0b10100000); // only default flag is set
}