#include <AlienCPUTest.h>

class SBCTest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};


// SBC IMMEDIATE TESTS
TEST_F(SBCTest, SubtractWithCarry_Immediate_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x0012); // value to subtract from accumulator
    cpu.setFlag(cpu.C_FLAG, true);
    cpu.A = 0x2047;

    TestInstruction(cpu, 3, 0x00001026);

    EXPECT_EQ(cpu.A, 0x2034) << "Accumulator should be decremented by 0x0012";
    TestUnchangedState(cpu, X, Y, SP, P);
}

TEST_F(SBCTest, SubtractWithCarry_Immediate_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x0000); // value to subtract from accumulator
    cpu.A = 0x0000;

    TestInstruction(cpu, 3, 0x00001026);

    EXPECT_EQ(cpu.A, 0x0000) << "Accumulator should be decremented by 0x0000";
    EXPECT_EQ(cpu.P, 0b00100010) << "Negative and zero flag should be set";
    TestUnchangedState(cpu, X, Y, SP);
}

TEST_F(SBCTest, SubtractWithCarry_Immediate_CARRYFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x0002); // value to subtract from accumulator
    cpu.A = 0x0001;

    TestInstruction(cpu, 3, 0x00001026);

    EXPECT_EQ(cpu.A, 0xFFFF) << "Accumulator should be decremented by 0x0002";
    EXPECT_EQ(cpu.P, 0b10100001) << "Negative and carry flag should be set";
    TestUnchangedState(cpu, X, Y, SP);
}


// SBC ABSOLUTE TESTS
TEST_F(SBCTest, SubtractWithCarry_Absolute_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012034); // address of value to subtract from accumulator
    cpu.writeTwoBytes(0x00012034, 0x0012); // value to subtract from accumulator
    cpu.setFlag(cpu.C_FLAG, true);
    cpu.A = 0x1247;

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.A, 0x1234) << "Accumulator should be decremented by 0x0012";
    TestUnchangedState(cpu, X, Y, SP, P);
}

TEST_F(SBCTest, SubtractWithCarry_Absolute_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012034); // address of value to subtract from accumulator
    cpu.writeTwoBytes(0x00012034, 0x0001); // value to subtract from accumulator
    cpu.A = 0x0001;

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.A, 0x0000) << "Accumulator should be decremented by 0x0001";
    EXPECT_EQ(cpu.P, 0b00100010) << "Negative and zero flag should be set";
    TestUnchangedState(cpu, X, Y, SP);
}

TEST_F(SBCTest, SubtractWithCarry_Absolute_CARRYFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012034); // address of value to subtract from accumulator
    cpu.writeTwoBytes(0x00012034, 0x0002); // value to subtract from accumulator
    cpu.A = 0x0001;

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.A, 0xFFFF) << "Accumulator should be decremented by 0x0002";
    EXPECT_EQ(cpu.P, 0b10100001) << "Negative and carry flag should be set";
    TestUnchangedState(cpu, X, Y, SP);
}


// SBC ABSOLUTE XINDEXED TESTS
TEST_F(SBCTest, SubtractWithCarry_AbsoluteXIndexed_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_ABS_X, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012034); // partial address of value to subtract from accumulator
    cpu.X = 0x0001;
    cpu.writeTwoBytes(0x00012035, 0x0012); // value to subtract from accumulator
    cpu.setFlag(cpu.C_FLAG, true);
    cpu.A = 0x1247;

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.A, 0x1234) << "Accumulator should be decremented by 0x0012";
    TestUnchangedState(cpu, Y, SP, P);
}

TEST_F(SBCTest, SubtractWithCarry_AbsoluteXIndexed_PAGECROSSING) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_ABS_X, 0x00001023);
    cpu.writeWord(0x00001024, 0x0001FFFF); // partial address of value to subtract from accumulator
    cpu.X = 0x0001;
    cpu.writeTwoBytes(0x00020000, 0x0012); // value to subtract from accumulator
    cpu.A = 0x1247;
    cpu.setFlag(cpu.C_FLAG, true);

    TestInstruction(cpu, 9, 0x00001028);

    EXPECT_EQ(cpu.A, 0x1234) << "Accumulator should be decremented by 0x0012";
    TestUnchangedState(cpu, Y, SP, P);
}

TEST_F(SBCTest, SubtractWithCarry_AbsoluteXIndexed_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_ABS_X, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012034); // partial address of value to subtract from accumulator
    cpu.X = 0x0001;
    cpu.writeTwoBytes(0x00012035, 0x0001); // value to subtract from accumulator
    cpu.A = 0x0001;

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.A, 0x0000) << "Accumulator should be decremented by 0x0001";
    EXPECT_EQ(cpu.P, 0b00100010) << "Negative and zero flag should be set";
    TestUnchangedState(cpu, Y, SP);
}

TEST_F(SBCTest, SubtractWithCarry_AbsoluteXIndexed_CARRYFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_ABS_X, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012034); // partial address of value to subtract from accumulator
    cpu.X = 0x0001;
    cpu.writeTwoBytes(0x00012035, 0x0002); // value to subtract from accumulator
    cpu.A = 0x0001;

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.A, 0xFFFF) << "Accumulator should be decremented by 0x0002";
    EXPECT_EQ(cpu.P, 0b10100001) << "Negative and carry flag should be set";
    TestUnchangedState(cpu, Y, SP);
}


// SBC ABSOLUTE YINDEXED TESTS
TEST_F(SBCTest, SubtractWithCarry_AbsoluteYIndexed_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_ABS_Y, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012034); // partial address of value to subtract from accumulator
    cpu.Y = 0x0001;
    cpu.writeTwoBytes(0x00012035, 0x0012); // value to subtract from accumulator
    cpu.setFlag(cpu.C_FLAG, true);
    cpu.A = 0x1247;

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.A, 0x1234) << "Accumulator should be decremented by 0x0012";
    EXPECT_EQ(cpu.Y, 0x0001) << "Y register should be unchanged";
    TestUnchangedState(cpu, X, SP, P);
}

TEST_F(SBCTest, SubtractWithCarry_AbsoluteYIndexed_PAGECROSSING) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_ABS_Y, 0x00001023);
    cpu.writeWord(0x00001024, 0x0001FFFF); // partial address of value to subtract from accumulator
    cpu.Y = 0x0001;
    cpu.writeTwoBytes(0x00020000, 0x0012); // value to subtract from accumulator
    cpu.setFlag(cpu.C_FLAG, true);
    cpu.A = 0x1247;

    TestInstruction(cpu, 9, 0x00001028);

    EXPECT_EQ(cpu.A, 0x1234) << "Accumulator should be decremented by 0x0012";
    EXPECT_EQ(cpu.Y, 0x0001) << "Y register should be unchanged";
    TestUnchangedState(cpu, X, SP, P);
}

TEST_F(SBCTest, SubtractWithCarry_AbsoluteYIndexed_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_ABS_Y, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012034); // partial address of value to subtract from accumulator
    cpu.Y = 0x0001;
    cpu.writeTwoBytes(0x00012035, 0x0001); // value to subtract from accumulator
    cpu.A = 0x0001;

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.A, 0x0000) << "Accumulator should be decremented by 0x0001";
    EXPECT_EQ(cpu.Y, 0x0001) << "Y register should be unchanged";
    EXPECT_EQ(cpu.P, 0b00100010) << "Negative and zero flag should be set";
    TestUnchangedState(cpu, X, SP);
}

TEST_F(SBCTest, SubtractWithCarry_AbsoluteYIndexed_CARRYFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_ABS_Y, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012034); // partial address of value to subtract from accumulator
    cpu.Y = 0x0001;
    cpu.writeTwoBytes(0x00012035, 0x0002); // value to subtract from accumulator
    cpu.A = 0x0001;

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.A, 0xFFFF) << "Accumulator should be decremented by 0x0002";
    EXPECT_EQ(cpu.Y, 0x0001) << "Y register should be unchanged";
    EXPECT_EQ(cpu.P, 0b10100001) << "Negative and carry flag should be set";
    TestUnchangedState(cpu, X, SP);
}


// SBC XINDEXED INDIRECT TESTS
TEST_F(SBCTest, SubtractWithCarry_XIndexedIndirect_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_X_IND, 0x00000123);
    cpu.writeTwoBytes(0x00000124, 0x1234); // partial zp address of address to value to subtract from accumulator
    cpu.X = 0x0001;
    cpu.writeWord(0x00001235, 0x00012345); // address of value to subtract from accumulator
    cpu.writeTwoBytes(0x00012345, 0x0012); // value to subtract from accumulator
    cpu.setFlag(cpu.C_FLAG, true);
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
    cpu.setFlag(cpu.C_FLAG, true);
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
    cpu.A = 0x0001;

    TestInstruction(cpu, 10, 0x00000126);

    EXPECT_EQ(cpu.A, 0x0000) << "Accumulator should be decremented by 0x0001";
    EXPECT_EQ(cpu.X, 0x0001) << "X register should be unchanged";
    EXPECT_EQ(cpu.P, 0b00100010) << "Negative and zero flag should be set";
    TestUnchangedState(cpu, Y, SP);
}

TEST_F(SBCTest, SubtractWithCarry_XIndexedIndirect_CARRYFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_X_IND, 0x00000123);
    cpu.writeTwoBytes(0x00000124, 0x1234); // partial zp address of address to value to subtract from accumulator
    cpu.X = 0x0001;
    cpu.writeWord(0x00001235, 0x00012345); // address of value to subtract from accumulator
    cpu.writeTwoBytes(0x00012345, 0x0001); // value to subtract from accumulator
    cpu.setFlag(cpu.C_FLAG, true);
    cpu.A = 0x0001;

    TestInstruction(cpu, 10, 0x00000126);

    EXPECT_EQ(cpu.A, 0xFFFF) << "Accumulator should be decremented by 0x0002";
    EXPECT_EQ(cpu.X, 0x0001) << "X register should be unchanged";
    EXPECT_EQ(cpu.P, 0b10100001) << "Negative and carry flag should be set";
    TestUnchangedState(cpu, Y, SP);
}


// SBC INDIRECT YINDEXED TESTS
TEST_F(SBCTest, SubtractWithCarry_IndirectYIndexed_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_IND_Y, 0x00011234);
    cpu.writeTwoBytes(0x00011235, 0x1234); // zp address of partial address of value to subtract from accumulator
    cpu.writeWord(0x00001234, 0x00012345); // partial address of value to subtract from accumulator
    cpu.Y = 0x0001;
    cpu.writeTwoBytes(0x00012346, 0x0012); // value to subtract from accumulator
    cpu.setFlag(cpu.C_FLAG, true);
    cpu.A = 0x1247;

    TestInstruction(cpu, 9, 0x00011237);

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
    cpu.setFlag(cpu.C_FLAG, true);
    cpu.A = 0x1247;

    TestInstruction(cpu, 11, 0x00011237);

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
    cpu.A = 0x0001;

    TestInstruction(cpu, 9, 0x00011237);

    EXPECT_EQ(cpu.A, 0x0000) << "Accumulator should be decremented by 0x0001";
    EXPECT_EQ(cpu.Y, 0x0001) << "Y register should be unchanged";
    EXPECT_EQ(cpu.P, 0b00100010) << "Negative and zero flag should be set";
    TestUnchangedState(cpu, X, SP);
}

TEST_F(SBCTest, SubtractWithCarry_IndirectYIndexed_CARRYFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_IND_Y, 0x00011234);
    cpu.writeTwoBytes(0x00011235, 0x1234); // zp address of partial address of value to subtract from accumulator
    cpu.writeWord(0x00001234, 0x00012345); // partial address of value to subtract from accumulator
    cpu.Y = 0x0001;
    cpu.writeTwoBytes(0x00012346, 0x0001); // value to subtract from accumulator
    cpu.setFlag(cpu.C_FLAG, true);
    cpu.A = 0x0001;

    TestInstruction(cpu, 9, 0x00011237);

    EXPECT_EQ(cpu.A, 0xFFFF) << "Accumulator should be decremented by 0x0002";
    EXPECT_EQ(cpu.Y, 0x0001) << "Y register should be unchanged";
    EXPECT_EQ(cpu.P, 0b10100001) << "Negative and carry flag should be set";
    TestUnchangedState(cpu, X, SP);
}


// SBC ZEROPAGE TESTS
TEST_F(SBCTest, SubtractWithCarry_ZeroPage_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_ZP, 0x00011234);
    cpu.writeTwoBytes(0x00011235, 0x1234); // zp address of value to subtract from accumulator
    cpu.writeTwoBytes(0x00001234, 0x0012); // value to subtract from accumulator
    cpu.setFlag(cpu.C_FLAG, true);
    cpu.A = 0x1247;

    TestInstruction(cpu, 5, 0x00011237);

    EXPECT_EQ(cpu.A, 0x1234) << "Accumulator should be decremented by 0x0012";
    TestUnchangedState(cpu, X, Y, SP, P);
}

TEST_F(SBCTest, SubtractWithCarry_ZeroPage_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_ZP, 0x00011234);
    cpu.writeTwoBytes(0x00011235, 0x1234); // zp address of value to subtract from accumulator
    cpu.writeTwoBytes(0x00001234, 0x0001); // value to subtract from accumulator
    cpu.A = 0x0001;

    TestInstruction(cpu, 5, 0x00011237);

    EXPECT_EQ(cpu.A, 0x0000) << "Accumulator should be decremented by 0x0001";
    EXPECT_EQ(cpu.P, 0b00100010) << "Negative and zero flag should be set";
    TestUnchangedState(cpu, X, Y, SP);
}

TEST_F(SBCTest, SubtractWithCarry_ZeroPage_CARRYFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_ZP, 0x00011234);
    cpu.writeTwoBytes(0x00011235, 0x1234); // zp address of value to subtract from accumulator
    cpu.writeTwoBytes(0x00001234, 0x0001); // value to subtract from accumulator
    cpu.setFlag(cpu.C_FLAG, true);
    cpu.A = 0x0001;

    TestInstruction(cpu, 5, 0x00011237);

    EXPECT_EQ(cpu.A, 0xFFFF) << "Accumulator should be decremented by 0x0002";
    EXPECT_EQ(cpu.P, 0b10100001) << "Negative and carry flag should be set";
    TestUnchangedState(cpu, X, Y, SP);
}


// SBC ZEROPAGE XINDEXED TESTS
TEST_F(SBCTest, SubtractWithCarry_ZeroPageXIndexed_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_ZP_X, 0x00011234);
    cpu.writeTwoBytes(0x00011235, 0x1234); // partial zp address of value to subtract from accumulator
    cpu.X = 0x0001;
    cpu.writeTwoBytes(0x00001235, 0x0012); // value to subtract from accumulator
    cpu.setFlag(cpu.C_FLAG, true);
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
    cpu.setFlag(cpu.C_FLAG, true);
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
    cpu.A = 0x0001;

    TestInstruction(cpu, 6, 0x00011237);

    EXPECT_EQ(cpu.A, 0x0000) << "Accumulator should be decremented by 0x0001";
    EXPECT_EQ(cpu.X, 0x0001) << "X register should be unchanged";
    EXPECT_EQ(cpu.P, 0b00100010) << "Negative and zero flag should be set";
    TestUnchangedState(cpu, Y, SP);
}

TEST_F(SBCTest, SubtractWithCarry_ZeroPageXIndexed_CARRYFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_SBC_ZP_X, 0x00011234);
    cpu.writeTwoBytes(0x00011235, 0x1234); // partial zp address of value to subtract from accumulator
    cpu.X = 0x0001;
    cpu.writeTwoBytes(0x00001235, 0x0001); // value to subtract from accumulator
    cpu.setFlag(cpu.C_FLAG, true);
    cpu.A = 0x0001;

    TestInstruction(cpu, 6, 0x00011237);

    EXPECT_EQ(cpu.A, 0xFFFF) << "Accumulator should be decremented by 0x0001";
    EXPECT_EQ(cpu.X, 0x0001) << "X register should be unchanged";
    EXPECT_EQ(cpu.P, 0b10100001) << "Negative and carry flag should be set";
    TestUnchangedState(cpu, Y, SP);
}