#include <AlienCPUTest.h>

class TYATest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};


// TYA IMPLIED TESTS
TEST_F(TYATest, TransferYToAccumulatorImplied_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_TYA_IMPL, 0x00001023);
    cpu.Y = 0x1342;
    cpu.A = 0x0034;

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.A, 0x1342) << "Accumulator should be set to the Y's value";
    EXPECT_EQ(cpu.Y, 0x1342) << "Y should not be altered";
    TestUnchangedState(cpu, X, SP, P);
}

TEST_F(TYATest, TransferYToAccumulatorImplied_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_TYA_IMPL, 0x00001023);
    cpu.Y = 0x0000;
    cpu.A = 0x0034;

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.A, 0x0000) << "Accumulator should be set to the Y's value";
    EXPECT_EQ(cpu.Y, 0x0000) << "Y should not be altered";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flag should be set";
    TestUnchangedState(cpu, X, SP);
}

TEST_F(TYATest, TransferYToAccumulatorImplied_NEGATIVEFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_TYA_IMPL, 0x00001023);
    cpu.Y = 0xFFFF;
    cpu.A = 0x0034;

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.A, 0xFFFF) << "Accumulator should be set to the Y's value";
    EXPECT_EQ(cpu.Y, 0xFFFF) << "Y should not be altered";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flag should be set";
    TestUnchangedState(cpu, X, SP);
}