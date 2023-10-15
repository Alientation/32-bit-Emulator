#include <AlienCPUTest.h>

class DECTest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};


// DEC ABSOLUTE TESTS
TEST_F(DECTest, Decrement_Absolute_Normal) {
    LoadInstruction(cpu, AlienCPU::INS_DEC_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012345); // address to decrement from
    cpu.writeByte(0x00012345, 0x12); // value to decrement

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.motherboard.ram[0x00012345], 0x11) << "Memory value should be decremented";
    TestUnchangedState(cpu, A, X, Y, SP, P);
}

TEST_F(DECTest, Decrement_Absolute_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_DEC_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012345); // address to decrement from
    cpu.writeByte(0x00012345, 0x01); // value to decrement

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.motherboard.ram[0x00012345], 0x00) << "Memory value should be decremented (underflow) to zero";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only the default and zero flag should be set";
    TestUnchangedState(cpu, A, X, Y, SP);
}