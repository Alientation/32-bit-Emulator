#include <AlienCPUTest.h>

class SBCTest : public testing::Test { // TODO: FIX THESE TESTS TO TEST ALL FLAGS
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};


TEST_F(SBCTest, SubtractWithCarry_BRUTEFORCE) {
    // -32768 - 32767 - 1
    LoadInstruction(cpu, AlienCPU::INS_SBC_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0b0111111111111111);
    cpu.A = 0b1000000000000000;

    TestInstruction(cpu, 3, 0x00001026);

    EXPECT_EQ(cpu.A, 0b0000000000000000);
    EXPECT_EQ(cpu.P & 0b11000001, 0b01000001);

    cpu.reset();

    // -32768 - 32767
    LoadInstruction(cpu, AlienCPU::INS_SBC_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0b0111111111111111);
    cpu.A = 0b1000000000000000;
    cpu.setFlag(cpu.C_FLAG, true);

    TestInstruction(cpu, 3, 0x00001026);

    EXPECT_EQ(cpu.A, 0b0000000000000001);
    EXPECT_EQ(cpu.P & 0b11000001, 0b01000001);

    cpu.reset();

    // -32768 - 1
    LoadInstruction(cpu, AlienCPU::INS_SBC_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0b0000000000000001);
    cpu.A = 0b1000000000000000;
    cpu.setFlag(cpu.C_FLAG, true);

    TestInstruction(cpu, 3, 0x00001026);

    EXPECT_EQ(cpu.A, 0b0111111111111111);
    EXPECT_EQ(cpu.P & 0b11000001, 0b01000001);

    cpu.reset();

    // -32768 - -32768 - 1
    LoadInstruction(cpu, AlienCPU::INS_SBC_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0b1000000000000000);
    cpu.A = 0b1000000000000000;

    TestInstruction(cpu, 3, 0x00001026);

    EXPECT_EQ(cpu.A, 0b1111111111111111);
    EXPECT_EQ(cpu.P & 0b11000001, 0b10000000);

    cpu.reset();

    // -32768 - -32768
    LoadInstruction(cpu, AlienCPU::INS_SBC_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0b1000000000000000);
    cpu.A = 0b1000000000000000;
    cpu.setFlag(cpu.C_FLAG, true);

    TestInstruction(cpu, 3, 0x00001026);

    EXPECT_EQ(cpu.A, 0b0000000000000000);
    EXPECT_EQ(cpu.P & 0b11000001, 0b00000001);

    cpu.reset();

    // -32768 - -1
    LoadInstruction(cpu, AlienCPU::INS_SBC_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0b1111111111111111);
    cpu.A = 0b1000000000000000;
    cpu.setFlag(cpu.C_FLAG, true);

    TestInstruction(cpu, 3, 0x00001026);

    EXPECT_EQ(cpu.A, 0b1000000000000001);
    EXPECT_EQ(cpu.P & 0b11000001, 0b10000000);

    cpu.reset();

    // -1 - 32767 - 1
    LoadInstruction(cpu, AlienCPU::INS_SBC_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0b0111111111111111);
    cpu.A = 0b1111111111111111;

    TestInstruction(cpu, 3, 0x00001026);

    EXPECT_EQ(cpu.A, 0b0111111111111111);
    EXPECT_EQ(cpu.P & 0b11000001, 0b01000001);

    cpu.reset();

    // -1 - 32767
    LoadInstruction(cpu, AlienCPU::INS_SBC_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0b0111111111111111);
    cpu.A = 0b1111111111111111;
    cpu.setFlag(cpu.C_FLAG, true);

    TestInstruction(cpu, 3, 0x00001026);

    EXPECT_EQ(cpu.A, 0b1000000000000000);
    EXPECT_EQ(cpu.P & 0b11000001, 0b10000001);

    cpu.reset();

    // 0 - 32767 - 1
    LoadInstruction(cpu, AlienCPU::INS_SBC_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0b0111111111111111);
    cpu.A = 0b0000000000000000;

    TestInstruction(cpu, 3, 0x00001026);

    EXPECT_EQ(cpu.A, 0b1000000000000000);
    EXPECT_EQ(cpu.P & 0b11000001, 0b10000000);

    cpu.reset();

    // 0 - 32767
    LoadInstruction(cpu, AlienCPU::INS_SBC_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0b0111111111111111);
    cpu.A = 0b0000000000000000;
    cpu.setFlag(cpu.C_FLAG, true);

    TestInstruction(cpu, 3, 0x00001026);

    EXPECT_EQ(cpu.A, 0b1000000000000001);
    EXPECT_EQ(cpu.P & 0b11000001, 0b10000000);

    cpu.reset();

    // 0 - 1
    LoadInstruction(cpu, AlienCPU::INS_SBC_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0b0000000000000001);
    cpu.A = 0b0000000000000000;
    cpu.setFlag(cpu.C_FLAG, true);

    TestInstruction(cpu, 3, 0x00001026);

    EXPECT_EQ(cpu.A, 0b1111111111111111);
    EXPECT_EQ(cpu.P & 0b11000001, 0b10000000);

    cpu.reset();

    // 0 - -32768 - 1
    LoadInstruction(cpu, AlienCPU::INS_SBC_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024,0b1000000000000000);
    cpu.A = 0b0000000000000000;

    TestInstruction(cpu, 3, 0x00001026);

    EXPECT_EQ(cpu.A, 0b0111111111111111);
    EXPECT_EQ(cpu.P & 0b11000001, 0b00000000);

    cpu.reset();

    // 0 - -32768
    LoadInstruction(cpu, AlienCPU::INS_SBC_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024,0b1000000000000000);
    cpu.A = 0b0000000000000000;
    cpu.setFlag(cpu.C_FLAG, true);

    TestInstruction(cpu, 3, 0x00001026);

    EXPECT_EQ(cpu.A, 0b1000000000000000);
    EXPECT_EQ(cpu.P & 0b11000001, 0b11000000);

    cpu.reset();

    // 32767 - 1
    LoadInstruction(cpu, AlienCPU::INS_SBC_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024,0b0000000000000001);
    cpu.A = 0b0111111111111111;
    cpu.setFlag(cpu.C_FLAG, true);

    TestInstruction(cpu, 3, 0x00001026);

    EXPECT_EQ(cpu.A, 0b0111111111111110);
    EXPECT_EQ(cpu.P & 0b11000001, 0b00000001);

    cpu.reset();

    // 32767 - 32767
    LoadInstruction(cpu, AlienCPU::INS_SBC_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024,0b0111111111111111);
    cpu.A = 0b0111111111111111;
    cpu.setFlag(cpu.C_FLAG, true);

    TestInstruction(cpu, 3, 0x00001026);

    EXPECT_EQ(cpu.A, 0b0000000000000000);
    EXPECT_EQ(cpu.P & 0b11000001, 0b00000001);

    cpu.reset();

    // 32767 - 32767 - 1
    LoadInstruction(cpu, AlienCPU::INS_SBC_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024,0b0111111111111111);
    cpu.A = 0b0111111111111111;

    TestInstruction(cpu, 3, 0x00001026);

    EXPECT_EQ(cpu.A, 0b1111111111111111);
    EXPECT_EQ(cpu.P & 0b11000001, 0b10000000);

    cpu.reset();

    // 32767 - -32768 - 1
    LoadInstruction(cpu, AlienCPU::INS_SBC_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024,0b1000000000000000);
    cpu.A = 0b0111111111111111;

    TestInstruction(cpu, 3, 0x00001026);

    EXPECT_EQ(cpu.A, 0b1111111111111110);
    EXPECT_EQ(cpu.P & 0b11000001, 0b11000000);

    cpu.reset();

    // 32767 - -32768
    LoadInstruction(cpu, AlienCPU::INS_SBC_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024,0b1000000000000000);
    cpu.A = 0b0111111111111111;
    cpu.setFlag(cpu.C_FLAG, true);

    TestInstruction(cpu, 3, 0x00001026);

    EXPECT_EQ(cpu.A, 0b1111111111111111);
    EXPECT_EQ(cpu.P & 0b11000001, 0b11000000);

    cpu.reset();
}


// SBC IMMEDIATE TESTS
TEST_F(SBCTest, SubtractWithCarry_Immediate_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x0012); // value to subtract from accumulator
    cpu.A = 0x2047;

    TestInstruction(cpu, 3, 0x00001026);

    EXPECT_EQ(cpu.A, 0x2034) << "Accumulator should be decremented by 0x0013";
    TestUnchangedState(cpu, X, Y, SP, P);
}

TEST_F(SBCTest, SubtractWithCarry_Immediate_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x0000); // value to subtract from accumulator
    cpu.setFlag(cpu.C_FLAG, true);
    cpu.A = 0x0000;

    TestInstruction(cpu, 3, 0x00001026);

    EXPECT_EQ(cpu.A, 0x0000) << "Accumulator should be decremented by 0x0000";
    EXPECT_EQ(cpu.P, 0b00100011) << "Default and zero flags should be set";
    TestUnchangedState(cpu, X, Y, SP);
}

TEST_F(SBCTest, SubtractWithCarry_Immediate_BORROWFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x0002); // value to subtract from accumulator
    cpu.setFlag(cpu.C_FLAG, true);
    cpu.A = 0x0001;

    TestInstruction(cpu, 3, 0x00001026);

    EXPECT_EQ(cpu.A, 0xFFFF) << "Accumulator should be decremented by 0x0002";
    EXPECT_EQ(cpu.P, 0b10100000) << "Negative and default flags should be set";
    TestUnchangedState(cpu, X, Y, SP);
}


// SBC ABSOLUTE TESTS
TEST_F(SBCTest, SubtractWithCarry_Absolute_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012034); // address of value to subtract from accumulator
    cpu.writeTwoBytes(0x00012034, 0x0012); // value to subtract from accumulator
    cpu.A = 0x1247;

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.A, 0x1234) << "Accumulator should be decremented by 0x0012";
    TestUnchangedState(cpu, X, Y, SP, P);
}

TEST_F(SBCTest, SubtractWithCarry_Absolute_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012034); // address of value to subtract from accumulator
    cpu.writeTwoBytes(0x00012034, 0x0001); // value to subtract from accumulator
    cpu.setFlag(cpu.C_FLAG, true);
    cpu.A = 0x0001;

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.A, 0x0000) << "Accumulator should be decremented by 0x0001";
    EXPECT_EQ(cpu.P, 0b00100011) << "Default, carry, and zero flags should be set";
    TestUnchangedState(cpu, X, Y, SP);
}

TEST_F(SBCTest, SubtractWithCarry_Absolute_BORROWFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012034); // address of value to subtract from accumulator
    cpu.writeTwoBytes(0x00012034, 0x0002); // value to subtract from accumulator
    cpu.setFlag(cpu.C_FLAG, true);
    cpu.A = 0x0001;

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.A, 0xFFFF) << "Accumulator should be decremented by 0x0002";
    EXPECT_EQ(cpu.P, 0b10100000) << "Negative and default flags should be set";
    TestUnchangedState(cpu, X, Y, SP);
}


// SBC ABSOLUTE XINDEXED TESTS
TEST_F(SBCTest, SubtractWithCarry_AbsoluteXIndexed_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_ABS_X, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012034); // partial address of value to subtract from accumulator
    cpu.X = 0x0001;
    cpu.writeTwoBytes(0x00012035, 0x0012); // value to subtract from accumulator
    cpu.A = 0x1247;

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.A, 0x1234) << "Accumulator should be decremented by 0x0012";
    TestUnchangedState(cpu, Y, SP, P);
}

TEST_F(SBCTest, SubtractWithCarry_AbsoluteXIndexed_PAGECROSSING) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_ABS_X, 0x00001023);
    cpu.writeWord(0x00001024, 0x0001FFFF); // partial address of value to subtract from accumulator
    cpu.X = 0x0001;
    cpu.writeTwoBytes(0x00020000, 0x0012); // value to subtract from accumulator
    cpu.A = 0x1247;

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.A, 0x1234) << "Accumulator should be decremented by 0x0012";
    TestUnchangedState(cpu, Y, SP, P);
}

TEST_F(SBCTest, SubtractWithCarry_AbsoluteXIndexed_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_ABS_X, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012034); // partial address of value to subtract from accumulator
    cpu.X = 0x0001;
    cpu.writeTwoBytes(0x00012035, 0x0001); // value to subtract from accumulator
    cpu.setFlag(cpu.C_FLAG, true);
    cpu.A = 0x0001;

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.A, 0x0000) << "Accumulator should be decremented by 0x0001";
    EXPECT_EQ(cpu.P, 0b00100011) << "Default, carry, and zero flags should be set";
    TestUnchangedState(cpu, Y, SP);
}

TEST_F(SBCTest, SubtractWithCarry_AbsoluteXIndexed_BORROWFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_ABS_X, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012034); // partial address of value to subtract from accumulator
    cpu.X = 0x0001;
    cpu.writeTwoBytes(0x00012035, 0x0002); // value to subtract from accumulator
    cpu.setFlag(cpu.C_FLAG, true);
    cpu.A = 0x0001;

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.A, 0xFFFF) << "Accumulator should be decremented by 0x0002";
    EXPECT_EQ(cpu.P, 0b10100000) << "Negative and default flags should be set";
    TestUnchangedState(cpu, Y, SP);
}


// SBC ABSOLUTE YINDEXED TESTS
TEST_F(SBCTest, SubtractWithCarry_AbsoluteYIndexed_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_ABS_Y, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012034); // partial address of value to subtract from accumulator
    cpu.Y = 0x0001;
    cpu.writeTwoBytes(0x00012035, 0x0012); // value to subtract from accumulator
    cpu.A = 0x1247;

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.A, 0x1234) << "Accumulator should be decremented by 0x0012";
    EXPECT_EQ(cpu.Y, 0x0001) << "Y register should be unchanged";
    TestUnchangedState(cpu, X, SP, P);
}

TEST_F(SBCTest, SubtractWithCarry_AbsoluteYIndexed_PAGECROSSING) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_ABS_Y, 0x00001023);
    cpu.writeWord(0x00001024, 0x0001FFFF); // partial address of value to subtract from accumulator
    cpu.Y = 0x0001;
    cpu.writeTwoBytes(0x00020000, 0x0012); // value to subtract from accumulator
    cpu.A = 0x1247;

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.A, 0x1234) << "Accumulator should be decremented by 0x0012";
    EXPECT_EQ(cpu.Y, 0x0001) << "Y register should be unchanged";
    TestUnchangedState(cpu, X, SP, P);
}

TEST_F(SBCTest, SubtractWithCarry_AbsoluteYIndexed_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_ABS_Y, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012034); // partial address of value to subtract from accumulator
    cpu.Y = 0x0001;
    cpu.writeTwoBytes(0x00012035, 0x0001); // value to subtract from accumulator
    cpu.setFlag(cpu.C_FLAG, true);
    cpu.A = 0x0001;

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.A, 0x0000) << "Accumulator should be decremented by 0x0001";
    EXPECT_EQ(cpu.Y, 0x0001) << "Y register should be unchanged";
    EXPECT_EQ(cpu.P, 0b00100011) << "Default, zero, and carry flags should be set";
    TestUnchangedState(cpu, X, SP);
}

TEST_F(SBCTest, SubtractWithCarry_AbsoluteYIndexed_BORROWFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_ABS_Y, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012034); // partial address of value to subtract from accumulator
    cpu.Y = 0x0001;
    cpu.writeTwoBytes(0x00012035, 0x0002); // value to subtract from accumulator
    cpu.setFlag(cpu.C_FLAG, true);
    cpu.A = 0x0001;

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.A, 0xFFFF) << "Accumulator should be decremented by 0x0002";
    EXPECT_EQ(cpu.Y, 0x0001) << "Y register should be unchanged";
    EXPECT_EQ(cpu.P, 0b10100000) << "Negative and default flags should be set";
    TestUnchangedState(cpu, X, SP);
}


// SBC XINDEXED INDIRECT TESTS
TEST_F(SBCTest, SubtractWithCarry_XIndexedIndirect_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_X_IND, 0x00000123);
    cpu.writeTwoBytes(0x00000124, 0x1234); // partial zp address of address to value to subtract from accumulator
    cpu.X = 0x0001;
    cpu.writeWord(0x00001235, 0x00012345); // address of value to subtract from accumulator
    cpu.writeTwoBytes(0x00012345, 0x0012); // value to subtract from accumulator
    cpu.A = 0x1247;

    TestInstruction(cpu, 10, 0x00000126);

    EXPECT_EQ(cpu.A, 0x1234) << "Accumulator should be decremented by 0x0012";
    EXPECT_EQ(cpu.X, 0x0001) << "X register should be unchanged";
    TestUnchangedState(cpu, Y, SP, P);
}

TEST_F(SBCTest, SubtractWithCarry_XIndexedIndirect_PAGEWRAP) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_X_IND, 0x00000123);
    cpu.writeTwoBytes(0x00000124, 0x0002); // partial zp address of address to value to subtract from accumulator
    cpu.X = 0xFFFF;
    cpu.writeWord(0x00000001, 0x00012345); // address of value to subtract from accumulator
    cpu.writeTwoBytes(0x00012345, 0x0012); // value to subtract from accumulator
    cpu.A = 0x1247;

    TestInstruction(cpu, 10, 0x00000126);

    EXPECT_EQ(cpu.A, 0x1234) << "Accumulator should be decremented by 0x0012";
    EXPECT_EQ(cpu.X, 0xFFFF) << "X register should be unchanged";
    TestUnchangedState(cpu, Y, SP, P);
}

TEST_F(SBCTest, SubtractWithCarry_XIndexedIndirect_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_X_IND, 0x00000123);
    cpu.writeTwoBytes(0x00000124, 0x1234); // partial zp address of address to value to subtract from accumulator
    cpu.X = 0x0001;
    cpu.writeWord(0x00001235, 0x00012345); // address of value to subtract from accumulator
    cpu.writeTwoBytes(0x00012345, 0x0001); // value to subtract from accumulator
    cpu.setFlag(cpu.C_FLAG, true);
    cpu.A = 0x0001;

    TestInstruction(cpu, 10, 0x00000126);

    EXPECT_EQ(cpu.A, 0x0000) << "Accumulator should be decremented by 0x0001";
    EXPECT_EQ(cpu.X, 0x0001) << "X register should be unchanged";
    EXPECT_EQ(cpu.P, 0b00100011) << "Default, zero, and carry flags should be set";
    TestUnchangedState(cpu, Y, SP);
}

TEST_F(SBCTest, SubtractWithCarry_XIndexedIndirect_BORROWFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_X_IND, 0x00000123);
    cpu.writeTwoBytes(0x00000124, 0x1234); // partial zp address of address to value to subtract from accumulator
    cpu.X = 0x0001;
    cpu.writeWord(0x00001235, 0x00012345); // address of value to subtract from accumulator
    cpu.writeTwoBytes(0x00012345, 0x0001); // value to subtract from accumulator
    cpu.A = 0x0001;

    TestInstruction(cpu, 10, 0x00000126);

    EXPECT_EQ(cpu.A, 0xFFFF) << "Accumulator should be decremented by 0x0002";
    EXPECT_EQ(cpu.X, 0x0001) << "X register should be unchanged";
    EXPECT_EQ(cpu.P, 0b10100000) << "Negative and default flags should be set";
    TestUnchangedState(cpu, Y, SP);
}


// SBC INDIRECT YINDEXED TESTS
TEST_F(SBCTest, SubtractWithCarry_IndirectYIndexed_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_IND_Y, 0x00011234);
    cpu.writeTwoBytes(0x00011235, 0x1234); // zp address of partial address of value to subtract from accumulator
    cpu.writeWord(0x00001234, 0x00012345); // partial address of value to subtract from accumulator
    cpu.Y = 0x0001;
    cpu.writeTwoBytes(0x00012346, 0x0012); // value to subtract from accumulator
    cpu.A = 0x1247;

    TestInstruction(cpu, 10, 0x00011237);

    EXPECT_EQ(cpu.A, 0x1234) << "Accumulator should be decremented by 0x0012";
    EXPECT_EQ(cpu.Y, 0x0001) << "Y register should be unchanged";
    TestUnchangedState(cpu, X, SP, P);
}

TEST_F(SBCTest, SubtractWithCarry_IndirectYIndexed_PAGECROSSING) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_IND_Y, 0x00011234);
    cpu.writeTwoBytes(0x00011235, 0x1234); // zp address of partial address of value to subtract from accumulator
    cpu.writeWord(0x00001234, 0x00010001); // partial address of value to subtract from accumulator
    cpu.Y = 0xFFFF;
    cpu.writeTwoBytes(0x00020000, 0x0012); // value to subtract from accumulator
    cpu.A = 0x1247;

    TestInstruction(cpu, 10, 0x00011237);

    EXPECT_EQ(cpu.A, 0x1234) << "Accumulator should be decremented by 0x0012";
    EXPECT_EQ(cpu.Y, 0xFFFF) << "Y register should be unchanged";
    TestUnchangedState(cpu, X, SP, P);
}

TEST_F(SBCTest, SubtractWithCarry_IndirectYIndexed_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_IND_Y, 0x00011234);
    cpu.writeTwoBytes(0x00011235, 0x1234); // zp address of partial address of value to subtract from accumulator
    cpu.writeWord(0x00001234, 0x00012345); // partial address of value to subtract from accumulator
    cpu.Y = 0x0001;
    cpu.writeTwoBytes(0x00012346, 0x0001); // value to subtract from accumulator
    cpu.setFlag(cpu.C_FLAG, true);
    cpu.A = 0x0001;

    TestInstruction(cpu, 10, 0x00011237);

    EXPECT_EQ(cpu.A, 0x0000) << "Accumulator should be decremented by 0x0001";
    EXPECT_EQ(cpu.Y, 0x0001) << "Y register should be unchanged";
    EXPECT_EQ(cpu.P, 0b00100011) << "Default, zero, and carry flags should be set";
    TestUnchangedState(cpu, X, SP);
}

TEST_F(SBCTest, SubtractWithCarry_IndirectYIndexed_CARRYFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_IND_Y, 0x00011234);
    cpu.writeTwoBytes(0x00011235, 0x1234); // zp address of partial address of value to subtract from accumulator
    cpu.writeWord(0x00001234, 0x00012345); // partial address of value to subtract from accumulator
    cpu.Y = 0x0001;
    cpu.writeTwoBytes(0x00012346, 0x0001); // value to subtract from accumulator
    cpu.A = 0x0001;

    TestInstruction(cpu, 10, 0x00011237);

    EXPECT_EQ(cpu.A, 0xFFFF) << "Accumulator should be decremented by 0x0002";
    EXPECT_EQ(cpu.Y, 0x0001) << "Y register should be unchanged";
    EXPECT_EQ(cpu.P, 0b10100000) << "Negative and default flags should be set";
    TestUnchangedState(cpu, X, SP);
}


// SBC ZEROPAGE TESTS
TEST_F(SBCTest, SubtractWithCarry_ZeroPage_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_ZP, 0x00011234);
    cpu.writeTwoBytes(0x00011235, 0x1234); // zp address of value to subtract from accumulator
    cpu.writeTwoBytes(0x00001234, 0x0012); // value to subtract from accumulator
    cpu.A = 0x1247;

    TestInstruction(cpu, 5, 0x00011237);

    EXPECT_EQ(cpu.A, 0x1234) << "Accumulator should be decremented by 0x0012";
    TestUnchangedState(cpu, X, Y, SP, P);
}

TEST_F(SBCTest, SubtractWithCarry_ZeroPage_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_ZP, 0x00011234);
    cpu.writeTwoBytes(0x00011235, 0x1234); // zp address of value to subtract from accumulator
    cpu.writeTwoBytes(0x00001234, 0x0001); // value to subtract from accumulator
    cpu.setFlag(cpu.C_FLAG, true);
    cpu.A = 0x0001;

    TestInstruction(cpu, 5, 0x00011237);

    EXPECT_EQ(cpu.A, 0x0000) << "Accumulator should be decremented by 0x0001";
    EXPECT_EQ(cpu.P, 0b00100011) << "Default, zero, and carry flags should be set";
    TestUnchangedState(cpu, X, Y, SP);
}

TEST_F(SBCTest, SubtractWithCarry_ZeroPage_CARRYFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_ZP, 0x00011234);
    cpu.writeTwoBytes(0x00011235, 0x1234); // zp address of value to subtract from accumulator
    cpu.writeTwoBytes(0x00001234, 0x0001); // value to subtract from accumulator
    cpu.A = 0x0001;

    TestInstruction(cpu, 5, 0x00011237);

    EXPECT_EQ(cpu.A, 0xFFFF) << "Accumulator should be decremented by 0x0002";
    EXPECT_EQ(cpu.P, 0b10100000) << "Negative and default flags should be set";
    TestUnchangedState(cpu, X, Y, SP);
}


// SBC ZEROPAGE XINDEXED TESTS
TEST_F(SBCTest, SubtractWithCarry_ZeroPageXIndexed_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_ZP_X, 0x00011234);
    cpu.writeTwoBytes(0x00011235, 0x1234); // partial zp address of value to subtract from accumulator
    cpu.X = 0x0001;
    cpu.writeTwoBytes(0x00001235, 0x0012); // value to subtract from accumulator
    cpu.A = 0x1247;

    TestInstruction(cpu, 6, 0x00011237);

    EXPECT_EQ(cpu.A, 0x1234) << "Accumulator should be decremented by 0x0012";
    EXPECT_EQ(cpu.X, 0x0001) << "X register should be unchanged";
    TestUnchangedState(cpu, Y, SP, P);
}

TEST_F(SBCTest, SubtractWithCarry_ZeroPageXIndexed_PAGEWRAPPING) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_ZP_X, 0x00011234);
    cpu.writeTwoBytes(0x00011235, 0x0001); // partial zp address of value to subtract from accumulator
    cpu.X = 0xFFFF;
    cpu.writeTwoBytes(0x00000000, 0x0012); // value to subtract from accumulator
    cpu.A = 0x1247;

    TestInstruction(cpu, 6, 0x00011237);

    EXPECT_EQ(cpu.A, 0x1234) << "Accumulator should be decremented by 0x0012";
    EXPECT_EQ(cpu.X, 0xFFFF) << "X register should be unchanged";
    TestUnchangedState(cpu, Y, SP, P);
}

TEST_F(SBCTest, SubtractWithCarry_ZeroPageXIndexed_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_ZP_X, 0x00011234);
    cpu.writeTwoBytes(0x00011235, 0x1234); // partial zp address of value to subtract from accumulator
    cpu.X = 0x0001;
    cpu.writeTwoBytes(0x00001235, 0x0001); // value to subtract from accumulator
    cpu.setFlag(cpu.C_FLAG, true);
    cpu.A = 0x0001;

    TestInstruction(cpu, 6, 0x00011237);

    EXPECT_EQ(cpu.A, 0x0000) << "Accumulator should be decremented by 0x0001";
    EXPECT_EQ(cpu.X, 0x0001) << "X register should be unchanged";
    EXPECT_EQ(cpu.P, 0b00100011) << "Default, zero, and carry flags should be set";
    TestUnchangedState(cpu, Y, SP);
}

TEST_F(SBCTest, SubtractWithCarry_ZeroPageXIndexed_CARRYFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_ZP_X, 0x00011234);
    cpu.writeTwoBytes(0x00011235, 0x1234); // partial zp address of value to subtract from accumulator
    cpu.X = 0x0001;
    cpu.writeTwoBytes(0x00001235, 0x0001); // value to subtract from accumulator
    cpu.A = 0x0001;

    TestInstruction(cpu, 6, 0x00011237);

    EXPECT_EQ(cpu.A, 0xFFFF) << "Accumulator should be decremented by 0x0001";
    EXPECT_EQ(cpu.X, 0x0001) << "X register should be unchanged";
    EXPECT_EQ(cpu.P, 0b10100000) << "Negative and default flags should be set";
    TestUnchangedState(cpu, Y, SP);
}