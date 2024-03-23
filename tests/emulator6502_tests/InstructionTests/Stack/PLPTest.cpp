#include <emulator6502_tests\AlienCPUTest.h>

class PLPTest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};


// PLP IMPLIED TESTS
TEST_F(PLPTest, PopProcessor_Implied_Normal) {
    LoadInstruction(cpu, AlienCPU::INS_PLP_IMPL, 0x00001023);
    cpu.P = 0b11111111;
    cpu.motherboard.ram[0x0001FFFF] = 0b01000101;
    cpu.SP = 0xFFFE;

    TestInstruction(cpu, 3, 0x00001024);

    EXPECT_EQ(cpu.P, 0b01110101) << "Processor status should be popped from stack with default and break flags set";
    EXPECT_EQ(cpu.SP, 0xFFFF) << "Stack pointer should be incremented";
    TestUnchangedState(cpu, A, X, Y, P);
}