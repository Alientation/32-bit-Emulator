#include <emulator6502_tests\AlienCPUTest.h>

class PLATest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};


// PLA IMPLIED TESTS
TEST_F(PLATest, PopAccumulator_Implied_Normal) {
    LoadInstruction(cpu, AlienCPU::INS_PLA_IMPL, 0x00001023);
    cpu.motherboard.ram[0x0001FFFE] = 0x34;
    cpu.motherboard.ram[0x0001FFFF] = 0x12;
    cpu.SP = 0xFFFD;

    TestInstruction(cpu, 4, 0x00001024);

    EXPECT_EQ(cpu.A, 0x1234) << "Accumulator should be set to the value on the stack";
    EXPECT_EQ(cpu.SP, 0xFFFF) << "Stack pointer should be incremented";
    TestUnchangedState(cpu, X, Y, P);
}