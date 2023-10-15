#include <AlienCPUTest.h>

class DEXTest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};


// DEX IMPLIED TESTS
TEST_F(DEXTest, DecrementXImplied_Normal) {
    LoadInstruction(cpu, AlienCPU::INS_DEX_IMPL, 0x00001023);
    cpu.X = 0x1234;

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.X, 0x1233) << "X should be decremented";
    TestUnchangedState(cpu, A, Y, SP, P);
}

TEST_F(DEXTest, DecrementXImplied_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_DEX_IMPL, 0x00001023);
    cpu.X = 0x0001;

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.X, 0x0000) << "X should be decremented (underflow) to zero";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flag should be set";
    TestUnchangedState(cpu, A, Y, SP);
}

TEST_F(DEXTest, DecrementXImplied_NEGATIVEFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_DEX_IMPL, 0x00001023);
    cpu.X = 0xFFFF;

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.X, 0xFFFE) << "X should be decremented";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flag should be set";
    TestUnchangedState(cpu, A, Y, SP);
}