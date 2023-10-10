#include <gtest/gtest.h>

#include <AlienCPUTest.h>

class TransferRegistersTest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};


// TAX Implied TESTS
TEST_F(TransferRegistersTest, Transfer_Accumulator_To_X) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_TAX_IMPL);
    cpu.A = 0x1342;
    cpu.X = 0x0034;

    cpu.start(2);

    // test X is set to the acccumulator's value
    EXPECT_EQ(cpu.X, 0x1342);
    EXPECT_EQ(cpu.A, 0x1342); // accumulator shouldn't change

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001024);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 2);
}


// TAY Implied TESTS
TEST_F(TransferRegistersTest, Transfer_Accumulator_To_Y) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_TAY_IMPL);
    cpu.A = 0x1342;
    cpu.Y = 0x0034;

    cpu.start(2);

    // test Y is set to the acccumulator's value
    EXPECT_EQ(cpu.Y, 0x1342);
    EXPECT_EQ(cpu.A, 0x1342); // accumulator shouldn't change

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001024);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 2);
}


// TSX Implied TESTS
TEST_F(TransferRegistersTest, Transfer_StackPointer_To_X) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_TSX_IMPL);
    cpu.SP = 0x1342;
    cpu.X = 0x0034;

    cpu.start(2);

    // test X is set to the stack pointer's value
    EXPECT_EQ(cpu.X, 0x1342);
    EXPECT_EQ(cpu.SP, 0x1342); // stack pointer shouldn't change

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001024);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 2);
}


// TXA Implied TESTS
TEST_F(TransferRegistersTest, Transfer_X_To_Accumulator) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_TXA_IMPL);
    cpu.X = 0x1342;
    cpu.A = 0x0034;

    cpu.start(2);

    // test acccumulator is set to the X's value
    EXPECT_EQ(cpu.A, 0x1342);
    EXPECT_EQ(cpu.X, 0x1342); // X shouldn't change

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001024);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 2);
}


// TYA Implied TESTS
TEST_F(TransferRegistersTest, Transfer_Y_To_Accumulator) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_TYA_IMPL);
    cpu.Y = 0x1342;
    cpu.A = 0x0034;

    cpu.start(2);

    // test acccumulator is set to the Y's value
    EXPECT_EQ(cpu.A, 0x1342);
    EXPECT_EQ(cpu.Y, 0x1342); // Y shouldn't change

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001024);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 2);
}


// TXS Implied TESTS
TEST_F(TransferRegistersTest, Transfer_X_To_StackPointer) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_TXS_IMPL);
    cpu.X = 0x1342;
    cpu.SP = 0x0034;

    cpu.start(2);

    // test stack pointer is set to the X's value
    EXPECT_EQ(cpu.SP, 0x1342);
    EXPECT_EQ(cpu.X, 0x1342); // X shouldn't change

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001024);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 2);
}