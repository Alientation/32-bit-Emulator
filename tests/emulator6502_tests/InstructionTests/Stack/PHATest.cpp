#include <emulator6502_tests\AlienCPUTest.h>

class PHATest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};


// PHA IMPLIED TESTS
TEST_F(PHATest, PushAccumulator_Implied_Normal) {
    LoadInstruction(cpu, AlienCPU::INS_PHA_IMPL, 0x00001023);
    cpu.A = 0x1234;

    TestInstruction(cpu, 4, 0x00001024);

    EXPECT_EQ(cpu.motherboard.ram[0x0001FFFE], 0x34) << "Accumulator low byte should be pushed to the stack";
    EXPECT_EQ(cpu.motherboard.ram[0x0001FFFF], 0x12) << "Accumulator high byte should be pushed to the stack";
    EXPECT_EQ(cpu.SP, 0xFFFD) << "Stack pointer should be decremented";
    EXPECT_EQ(cpu.A, 0x1234) << "Accumulator should not be altered";
    TestUnchangedState(cpu, X, Y, P);
}
