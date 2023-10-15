#include <AlienCPUTest.h>

class DEYTest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};


// DEY IMPLIED TESTS
TEST_F(DEYTest, DecrementY_Implied_Normal) {
    LoadInstruction(cpu, AlienCPU::INS_DEY_IMPL, 0x00001023);
    cpu.Y = 0x1234;

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.Y, 0x1233) << "Y should be decremented";
    TestUnchangedState(cpu, A, X, SP, P);
}

TEST_F(DEYTest, DecrementY_Implied_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_DEY_IMPL, 0x00001023);
    cpu.Y = 0x0001;

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.Y, 0x0000) << "Y should be decremented (underflow) to zero";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flag should be set";
    TestUnchangedState(cpu, A, X, SP);
}

TEST_F(DEYTest, DecrementY_Implied_NEGATIVEFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_DEY_IMPL, 0x00001023);
    cpu.Y = 0xFFFF;

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.Y, 0xFFFE) << "Y should be decremented";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flag should be set";
    TestUnchangedState(cpu, A, X, SP);
}