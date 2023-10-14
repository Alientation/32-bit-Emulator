#include <AlienCPUTest.h>

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
TEST_F(PLPTest, PopProcessorImplied_Normal) {
    LoadInstruction(cpu, AlienCPU::INS_PLP_IMPL, 0x00001023);
    cpu.motherboard.ram[0x0001FFFF] = 0b01010101;
    cpu.SP = 0xFFFE;

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.P, 0b01010101) << "Processor status should be set to the value on the stack";
    EXPECT_EQ(cpu.SP, 0xFFFF) << "Stack pointer should be incremented";
    TestUnchangedState(cpu, A, X, Y, P);
}