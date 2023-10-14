#include <AlienCPUTest.h>

class PHPTest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};


// PHP IMPLIED TESTS
TEST_F(PHPTest, PushProcessorImplied_Normal) {
    LoadInstruction(cpu, AlienCPU::INS_PHP_IMPL, 0x00001023);
    cpu.P = 0b01010101;

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.motherboard.ram[0x0001FFFF], 0b01010101) << "Processor status should be pushed to the stack";
    EXPECT_EQ(cpu.SP, 0xFFFE) << "Stack pointer should be decremented";
    EXPECT_EQ(cpu.P, 0b01010101) << "Processor status should not be altered";
    TestUnchangedState(cpu, A, X, Y);
}