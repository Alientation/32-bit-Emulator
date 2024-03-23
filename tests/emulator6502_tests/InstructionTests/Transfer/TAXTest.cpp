#include <emulator6502_tests\AlienCPUTest.h>

class TAXTest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};


// TAX IMPLIED TESTS
TEST_F(TAXTest, TransferAccumulatorToX_Implied_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_TAX_IMPL, 0x00001023);
    cpu.A = 0x1342;
    cpu.X = 0x0034;

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.X, 0x1342) << "X should be set to the Accumulator's value";
    EXPECT_EQ(cpu.A, 0x1342) << "Accumulator should not be altered";
    TestUnchangedState(cpu, Y, SP, P);
}

TEST_F(TAXTest, TransferAccumulatorToX_Implied_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_TAX_IMPL, 0x00001023);
    cpu.A = 0x0000;
    cpu.X = 0x0034;

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.X, 0x0000) << "X should be set to the Accumulator's value";
    EXPECT_EQ(cpu.A, 0x0000) << "Accumulator should not be altered";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flag should be set";
    TestUnchangedState(cpu, Y, SP);
}

TEST_F(TAXTest, TransferAccumulatorToX_Implied_NEGATIVEFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_TAX_IMPL, 0x00001023);
    cpu.A = 0xFFFF;
    cpu.X = 0x0034;

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.X, 0xFFFF) << "X should be set to the Accumulator's value";
    EXPECT_EQ(cpu.A, 0xFFFF) << "Accumulator should not be altered";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flag should be set";
    TestUnchangedState(cpu, Y, SP);
}