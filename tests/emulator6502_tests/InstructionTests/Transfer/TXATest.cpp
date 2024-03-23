#include <emulator6502_tests\AlienCPUTest.h>

class TXATest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};


// TXA IMPLIED TESTS
TEST_F(TXATest, TransferXToAccumulator_Implied_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_TXA_IMPL, 0x00001023);
    cpu.X = 0x1342;
    cpu.A = 0x0034;

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.A, 0x1342) << "Accumulator should be set to the X's value";
    EXPECT_EQ(cpu.X, 0x1342) << "X should not be altered";
    TestUnchangedState(cpu, Y, SP, P);
}

TEST_F(TXATest, TransferXToAccumulator_Implied_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_TXA_IMPL, 0x00001023);
    cpu.X = 0x0000;
    cpu.A = 0x0034;

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.A, 0x0000) << "Accumulator should be set to the X's value";
    EXPECT_EQ(cpu.X, 0x0000) << "X should not be altered";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flag should be set";
    TestUnchangedState(cpu, Y, SP);
}

TEST_F(TXATest, TransferXToAccumulator_Implied_NEGATIVEFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_TXA_IMPL, 0x00001023);
    cpu.X = 0xFFFF;
    cpu.A = 0x0034;

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.A, 0xFFFF) << "Accumulator should be set to the X's value";
    EXPECT_EQ(cpu.X, 0xFFFF) << "X should not be altered";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flag should be set";
    TestUnchangedState(cpu, Y, SP);
}