#include <gtest/gtest.h>

#include <AlienCPUTest.h>

class TXSTest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};


TEST_F(TXSTest, TransferXToStackPointerImplied_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_TXS_IMPL);
    cpu.X = 0x1342;
    cpu.SP = 0x0034;

    cpu.start(2);

    EXPECT_EQ(cpu.SP, 0x1342); // test stack pointer is set to the X's value
    EXPECT_EQ(cpu.X, 0x1342); // X shouldn't change
    EXPECT_EQ(cpu.P, 0b00100000); // test only default flag is set
    EXPECT_EQ(cpu.PC, 0x00001024); // test PC is at next instruction
    EXPECT_EQ(cpu.cycles, 2); // test cycle counter
}

TEST_F(TXSTest, TransferXToStackPointerImplied_ZEROFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_TXS_IMPL);
    cpu.X = 0x0000;
    cpu.SP = 0x0034;

    cpu.start(2);

    EXPECT_EQ(cpu.SP, 0x0000); // test stack pointer is set to the X's value
    EXPECT_EQ(cpu.X, 0x0000); // X shouldn't change
    EXPECT_EQ(cpu.P, 0b00100010); // test only default and zero flag is set
    EXPECT_EQ(cpu.PC, 0x00001024); // test PC is at next instruction
    EXPECT_EQ(cpu.cycles, 2); // test cycle counter
}

TEST_F(TXSTest, TransferXToStackPointerImplied_NEGATIVEFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_TXS_IMPL);
    cpu.X = 0xFFFF;
    cpu.SP = 0x0034;

    cpu.start(2);

    EXPECT_EQ(cpu.SP, 0xFFFF); // test stack pointer is set to the X's value
    EXPECT_EQ(cpu.X, 0xFFFF); // X shouldn't change
    EXPECT_EQ(cpu.P, 0b10100000); // test only default and negative flag is set
    EXPECT_EQ(cpu.PC, 0x00001024); // test PC is at next instruction
    EXPECT_EQ(cpu.cycles, 2); // test cycle counter
}