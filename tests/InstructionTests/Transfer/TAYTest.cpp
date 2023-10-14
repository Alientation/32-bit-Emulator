#include <AlienCPUTest.h>

class TAYTest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};


// TAY IMPLIED TESTS
TEST_F(TAYTest, TransferAccumulatorToYImplied_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_TAY_IMPL, 0x00001023);
    cpu.A = 0x1342;
    cpu.Y = 0x0034;

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.Y, 0x1342) << "Y should be set to the Accumulator's value";
    EXPECT_EQ(cpu.A, 0x1342) << "Accumulator should not be altered";
    TestUnchangedState(cpu, X, SP, P);
}

TEST_F(TAYTest, TransferAccumulatorToYImplied_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_TAY_IMPL, 0x00001023);
    cpu.A = 0x0000;
    cpu.Y = 0x0034;

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.Y, 0x0000) << "Y should be set to the Accumulator's value";
    EXPECT_EQ(cpu.A, 0x0000) << "Accumulator should not be altered";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flag should be set";
    TestUnchangedState(cpu, X, SP);
}

TEST_F(TAYTest, TransferAccumulatorToYImplied_NEGATIVEFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_TAY_IMPL, 0x00001023);
    cpu.A = 0xFFFF;
    cpu.Y = 0x0034;

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.Y, 0xFFFF) << "Y should be set to the Accumulator's value";
    EXPECT_EQ(cpu.A, 0xFFFF) << "Accumulator should not be altered";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flag should be set";
    TestUnchangedState(cpu, X, SP);
}