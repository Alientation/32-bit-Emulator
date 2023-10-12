#include <gtest/gtest.h>

#include <AlienCPUTest.h>

class TAXTest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};

TEST_F(TAXTest, TransferAccumulatorToXImplied_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_TAX_IMPL);
    cpu.A = 0x1342;
    cpu.X = 0x0034;

    cpu.start(2);

    EXPECT_EQ(cpu.X, 0x1342); // test X is set to the acccumulator's value
    EXPECT_EQ(cpu.A, 0x1342); // accumulator shouldn't change
    EXPECT_EQ(cpu.P, 0b00100000); // test only default flag is set
    EXPECT_EQ(cpu.PC, 0x00001024); // test PC is at next instruction
    EXPECT_EQ(cpu.cycles, 2); // test cycle counter
}

TEST_F(TAXTest, TransferAccumulatorToXImplied_ZEROFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_TAX_IMPL);
    cpu.A = 0x0000;
    cpu.X = 0x0034;

    cpu.start(2);

    EXPECT_EQ(cpu.X, 0x0000); // test X is set to the acccumulator's value
    EXPECT_EQ(cpu.A, 0x0000); // accumulator shouldn't change
    EXPECT_EQ(cpu.P, 0b00100010); // test only default and zero flag is set
    EXPECT_EQ(cpu.PC, 0x00001024); // test PC is at next instruction
    EXPECT_EQ(cpu.cycles, 2); // test cycle counter
}

TEST_F(TAXTest, TransferAccumulatorToXImplied_NEGATIVEFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_TAX_IMPL);
    cpu.A = 0xFFFF;
    cpu.X = 0x0034;

    cpu.start(2);

    EXPECT_EQ(cpu.X, 0xFFFF); // test X is set to the acccumulator's value
    EXPECT_EQ(cpu.A, 0xFFFF); // accumulator shouldn't change
    EXPECT_EQ(cpu.P, 0b10100000); // test only default and negative flag is set
    EXPECT_EQ(cpu.PC, 0x00001024); // test PC is at next instruction
    EXPECT_EQ(cpu.cycles, 2); // test cycle counter
}