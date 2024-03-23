#include <emulator6502_tests\AlienCPUTest.h>

class TXSTest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};


// TXS IMPLIED TESTS
TEST_F(TXSTest, TransferXToStackPointer_Implied_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_TXS_IMPL, 0x00001023);
    cpu.X = 0x1342;
    cpu.SP = 0x0034;

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.SP, 0x1342) << "Stack Pointer should be set to the X's value";
    EXPECT_EQ(cpu.X, 0x1342) << "X should not be altered";
    TestUnchangedState(cpu, A, Y, P);
}

TEST_F(TXSTest, TransferXToStackPointer_Implied_NOZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_TXS_IMPL, 0x00001023);
    cpu.X = 0x0000;
    cpu.SP = 0x0034;

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.SP, 0x0000) << "Stack Pointer should be set to the X's value";
    EXPECT_EQ(cpu.X, 0x0000) << "X should not be altered";
    TestUnchangedState(cpu, A, Y, P);
}

TEST_F(TXSTest, TransferXToStackPointer_Implied_NONEGATIVEFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_TXS_IMPL, 0x00001023);
    cpu.X = 0xFFFF;
    cpu.SP = 0x0034;

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.SP, 0xFFFF) << "Stack Pointer should be set to the X's value";
    EXPECT_EQ(cpu.X, 0xFFFF) << "X should not be altered";
    TestUnchangedState(cpu, A, Y, P);
}