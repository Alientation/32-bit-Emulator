#include <AlienCPUTest.h>

class INXTest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};


// INX IMPLIED TESTS
TEST_F(INXTest, IncrementXImplied_Normal) {
    LoadInstruction(cpu, AlienCPU::INS_INX_IMPL, 0x00001023);
    cpu.X = 0x1234;

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.X, 0x1235) << "X should be incremented";
    TestUnchangedState(cpu, A, Y, SP, P);
}

TEST_F(INXTest, IncrementXImplied_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_INX_IMPL, 0x00001023);
    cpu.X = 0xFFFF;

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.X, 0x0000) << "X should be incremented (overflow) to zero";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flag should be set";
    TestUnchangedState(cpu, A, Y, SP);
}

TEST_F(INXTest, IncrementXImplied_NEGATIVEFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_INX_IMPL, 0x00001023);
    cpu.X = 0xFFFE;

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.X, 0xFFFF) << "X should be incremented";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flag should be set";
    TestUnchangedState(cpu, A, Y, SP);
}