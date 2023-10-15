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


// INY IMPLIED TESTS
TEST_F(INYTest, IncrementYImplied_Normal) {
    LoadInstruction(cpu, AlienCPU::INS_INY_IMPL, 0x00001023);
    cpu.Y = 0x1234;

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.Y, 0x1235) << "Y should be incremented";
    TestUnchangedState(cpu, A, X, SP, P);
}

TEST_F(INYTest, IncrementYImplied_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_INY_IMPL, 0x00001023);
    cpu.Y = 0xFFFF;

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.Y, 0x0000) << "Y should be incremented (overflow) to zero";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flag should be set";
    TestUnchangedState(cpu, A, X, SP);
}

TEST_F(INYTest, IncrementYImplied_NEGATIVEFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_INY_IMPL, 0x00001023);
    cpu.Y = 0xFFFE;

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.Y, 0xFFFF) << "Y should be incremented";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flag should be set";
    TestUnchangedState(cpu, A, X, SP);
}