#include <AlienCPUTest.h>

class TSXTest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};


// TSX IMPLIED TESTS
TEST_F(TSXTest, TransferStackPointerToXImplied_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_TSX_IMPL, 0x00001023);
    cpu.SP = 0x1342;
    cpu.X = 0x0034;

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.X, 0x1342) << "X should be set to the Stack Pointer's value";
    EXPECT_EQ(cpu.SP, 0x1342) << "Stack Pointer should not be altered";
    TestUnchangedState(cpu, A, Y, P);
}

TEST_F(TSXTest, TransferStackPointerToXImplied_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_TSX_IMPL, 0x00001023);
    cpu.SP = 0x0000;
    cpu.X = 0x0034;

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.X, 0x0000) << "X should be set to the Stack Pointer's value";
    EXPECT_EQ(cpu.SP, 0x0000) << "Stack Pointer should not be altered";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flag should be set";
    TestUnchangedState(cpu, A, Y);
}

TEST_F(TSXTest, TransferStackPointerToXImplied_NEGATIVEFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_TSX_IMPL, 0x00001023);
    cpu.SP = 0xFFFF;
    cpu.X = 0x0034;

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.X, 0xFFFF) << "X should be set to the Stack Pointer's value";
    EXPECT_EQ(cpu.SP, 0xFFFF) << "Stack Pointer should not be altered";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flag should be set";
    TestUnchangedState(cpu, A, Y);
}
