#include <emulator6502_tests\AlienCPUTest.h>

class INCTest : public testing::Test { // TODO: FINISH WRITING TESTS FOR OTHER ADDRESSING MODES
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};


// INC ABSOLUTE TESTS
TEST_F(INCTest, Increment_Absolute_Normal) {
    LoadInstruction(cpu, AlienCPU::INS_INC_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012345); // address to increment from
    cpu.writeByte(0x00012345, 0x12); // value to increment

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.motherboard.ram[0x00012345], 0x13); // check incremented memory value
    TestUnchangedState(cpu, A, X, Y, SP, P);
}

TEST_F(INCTest, Increment_Absolute_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_INC_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012345); // address to increment from
    cpu.writeByte(0x00012345, 0xFF); // value to increment

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.motherboard.ram[0x00012345], 0x00); // check incremented memory value
    TestUnchangedState(cpu, A, X, Y, SP, P);
}