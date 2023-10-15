#include <AlienCPUTest.h>

class ANDTest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};


// AND IMMEDIATE TESTS
TEST_F(ANDTest, And_Immediate_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_AND_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1234); // value to bitwise and with accumulator
    cpu.A = 0x5678;

    TestInstruction(cpu, 3, 0x00001026);

    EXPECT_EQ(cpu.A, 0x1234 & 0x5678) << "Accumulator should be bitwise & with value";
    TestUnchangedState(cpu, X, Y, SP, P);
}

TEST_F(ANDTest, And_Immediate_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_AND_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x0000); // value to bitwise and with accumulator
    cpu.A = 0x5678;

    TestInstruction(cpu, 3, 0x00001026);

    EXPECT_EQ(cpu.A, 0x0000 & 0x5678) << "Accumulator should be bitwise & with value";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flag should be set";
    TestUnchangedState(cpu, X, Y, SP);
}

TEST_F(ANDTest, And_Immediate_NEGATIVEFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_AND_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0xFF34); // value to bitwise and with accumulator
    cpu.A = 0xFF78;

    TestInstruction(cpu, 3, 0x00001026);

    EXPECT_EQ(cpu.A, 0xFF34 & 0xFF78) << "Accumulator should be bitwise & with value";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flag should be set";
    TestUnchangedState(cpu, X, Y, SP);
}


// AND ABSOLUTE TESTS
TEST_F(ANDTest, And_Absolute_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_AND_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012345); // address of value to bitwise and with accumulator
    cpu.writeTwoBytes(0x00012345, 0x1234); // value to bitwise and with accumulator
    cpu.A = 0x5678;

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.A, 0x1234 & 0x5678) << "Accumulator should be bitwise & with value";
    TestUnchangedState(cpu, X, Y, SP, P);
}

TEST_F(ANDTest, And_Absolute_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_AND_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012345); // address of value to bitwise and with accumulator
    cpu.writeTwoBytes(0x00012345, 0x0000); // value to bitwise and with accumulator
    cpu.A = 0x5678;

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.A, 0x0000 & 0x5678) << "Accumulator should be bitwise & with value";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flag should be set";
    TestUnchangedState(cpu, X, Y, SP);
}

TEST_F(ANDTest, And_Absolute_NEGATIVEFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_AND_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012345); // address of value to bitwise and with accumulator
    cpu.writeTwoBytes(0x00012345, 0xFF34); // value to bitwise and with accumulator
    cpu.A = 0xFF78;

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.A, 0xFF34 & 0xFF78) << "Accumulator should be bitwise & with value";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flag should be set";
    TestUnchangedState(cpu, X, Y, SP);
}


// AND ABSOLUTE XINDEXED TESTS
TEST_F(ANDTest, And_AbsoluteXIndexed_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_AND_ABS_X, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012345); // partial address of value to bitwise and with accumulator
    cpu.X = 0x0002;
    cpu.writeTwoBytes(0x00012347, 0x1234); // value to bitwise and with accumulator
    cpu.A = 0x5678;

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.A, 0x1234 & 0x5678) << "Accumulator should be bitwise & with value";
    EXPECT_EQ(cpu.X, 0x0002) << "X register should not be altered";
    TestUnchangedState(cpu, Y, SP, P);
}

TEST_F(ANDTest, And_AbsoluteXIndexed_PAGECROSS) {
    LoadInstruction(cpu, AlienCPU::INS_AND_ABS_X, 0x00001023);
    cpu.writeWord(0x00001024, 0x00000001); // partial address of value to bitwise and with accumulator
    cpu.X = 0xFFFF;
    cpu.writeTwoBytes(0x00010000, 0x1234); // value to bitwise and with accumulator
    cpu.A = 0x5678;

    TestInstruction(cpu, 9, 0x00001028);

    EXPECT_EQ(cpu.A, 0x1234 & 0x5678) << "Accumulator should be bitwise & with value";
    EXPECT_EQ(cpu.X, 0xFFFF) << "X register should not be altered";
    TestUnchangedState(cpu, Y, SP, P);
}

TEST_F(ANDTest, And_AbsoluteXIndexed_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_AND_ABS_X, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012345); // partial address of value to bitwise and with accumulator
    cpu.X = 0x0002;
    cpu.writeTwoBytes(0x00012347, 0x0000); // value to bitwise and with accumulator
    cpu.A = 0x5678;

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.A, 0x0000 & 0x5678) << "Accumulator should be bitwise & with value";
    EXPECT_EQ(cpu.X, 0x0002) << "X register should not be altered";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flag should be set";
    TestUnchangedState(cpu, Y, SP);
}

TEST_F(ANDTest, And_AbsoluteXIndexed_NEGATIVEFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_AND_ABS_X, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012345); // partial address of value to bitwise and with accumulator
    cpu.X = 0x0002;
    cpu.writeTwoBytes(0x00012347, 0xFF34); // value to bitwise and with accumulator
    cpu.A = 0xFF78;

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.A, 0xFF34 & 0xFF78) << "Accumulator should be bitwise & with value";
    EXPECT_EQ(cpu.X, 0x0002) << "X register should not be altered";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flag should be set";
    TestUnchangedState(cpu, Y, SP);
}


// AND ABSOLUTE YINDEXED TESTS
TEST_F(ANDTest, And_AbsoluteYIndexed_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_AND_ABS_Y, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012345); // partial address of value to bitwise and with accumulator
    cpu.Y = 0x0002;
    cpu.writeTwoBytes(0x00012347, 0x1234); // value to bitwise and with accumulator
    cpu.A = 0x5678;

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.A, 0x1234 & 0x5678) << "Accumulator should be bitwise & with value";
    EXPECT_EQ(cpu.Y, 0x0002) << "Y register should not be altered";
    TestUnchangedState(cpu, X, SP, P);
}

TEST_F(ANDTest, And_AbsoluteYIndexed_PAGECROSS) {
    LoadInstruction(cpu, AlienCPU::INS_AND_ABS_Y, 0x00001023);
    cpu.writeWord(0x00001024, 0x00000001); // partial address of value to bitwise and with accumulator
    cpu.Y = 0xFFFF;
    cpu.writeTwoBytes(0x00010000, 0x1234); // value to bitwise and with accumulator
    cpu.A = 0x5678;

    TestInstruction(cpu, 9, 0x00001028);

    EXPECT_EQ(cpu.A, 0x1234 & 0x5678) << "Accumulator should be bitwise & with value";
    EXPECT_EQ(cpu.Y, 0xFFFF) << "Y register should not be altered";
    TestUnchangedState(cpu, X, SP, P);
}

TEST_F(ANDTest, And_AbsoluteYIndexed_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_AND_ABS_Y, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012345); // partial address of value to bitwise and with accumulator
    cpu.Y = 0x0002;
    cpu.writeTwoBytes(0x00012347, 0x0000); // value to bitwise and with accumulator
    cpu.A = 0x5678;

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.A, 0x0000 & 0x5678) << "Accumulator should be bitwise & with value";
    EXPECT_EQ(cpu.Y, 0x0002) << "Y register should not be altered";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flag should be set";
    TestUnchangedState(cpu, X, SP);
}

TEST_F(ANDTest, And_AbsoluteYIndexed_NEGATIVEFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_AND_ABS_Y, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012345); // partial address of value to bitwise and with accumulator
    cpu.Y = 0x0002;
    cpu.writeTwoBytes(0x00012347, 0xFF34); // value to bitwise and with accumulator
    cpu.A = 0xFF78;

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.A, 0xFF34 & 0xFF78) << "Accumulator should be bitwise & with value";
    EXPECT_EQ(cpu.Y, 0x0002) << "Y register should not be altered";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flag should be set";
    TestUnchangedState(cpu, X, SP);
}


// AND XINDEXED INDIRECT TESTS
TEST_F(ANDTest, And_XIndexedIndirect_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_AND_X_IND, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1234); // partial zp address of address of value to bitwise and with accumulator
    cpu.X = 0x0002;
    cpu.writeWord(0x00001236, 0x00012345); // address of value to bitwise and with accumulator
    cpu.writeTwoBytes(0x00012345, 0x1234); // value to bitwise and with accumulator
    cpu.A = 0x5678;

    TestInstruction(cpu, 10, 0x00001026);

    EXPECT_EQ(cpu.A, 0x1234 & 0x5678) << "Accumulator should be bitwise & with value";
    EXPECT_EQ(cpu.X, 0x0002) << "X register should not be altered";
    TestUnchangedState(cpu, Y, SP, P);
}

TEST_F(ANDTest, And_XIndexedIndirect_WRAPAROUND) {
    LoadInstruction(cpu, AlienCPU::INS_AND_X_IND, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1234); // partial zp address of address of value to bitwise and with accumulator
    cpu.X = 0xF002;
    cpu.writeWord(0x00000236, 0x00012345); // address of value to bitwise and with accumulator
    cpu.writeTwoBytes(0x00012345, 0x1234); // value to bitwise and with accumulator
    cpu.A = 0x5678;

    TestInstruction(cpu, 10, 0x00001026);

    EXPECT_EQ(cpu.A, 0x1234 & 0x5678) << "Accumulator should be bitwise & with value";
    EXPECT_EQ(cpu.X, 0xF002) << "X register should not be altered";
    TestUnchangedState(cpu, Y, SP, P);
}

TEST_F(ANDTest, And_XIndexedIndirect_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_AND_X_IND, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1234); // partial zp address of address of value to bitwise and with accumulator
    cpu.X = 0x0002;
    cpu.writeWord(0x00001236, 0x00012345); // address of value to bitwise and with accumulator
    cpu.writeTwoBytes(0x00012345, 0x0000); // value to bitwise and with accumulator
    cpu.A = 0x5678;

    TestInstruction(cpu, 10, 0x00001026);

    EXPECT_EQ(cpu.A, 0x0000 & 0x5678) << "Accumulator should be bitwise & with value";
    EXPECT_EQ(cpu.X, 0x0002) << "X register should not be altered";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flag should be set";
    TestUnchangedState(cpu, Y, SP);
}

TEST_F(ANDTest, And_XIndexedIndirect_NEGATIVEFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_AND_X_IND, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1234); // partial zp address of address of value to bitwise and with accumulator
    cpu.X = 0x0002;
    cpu.writeWord(0x00001236, 0x00012345); // address of value to bitwise and with accumulator
    cpu.writeTwoBytes(0x00012345, 0xFF34); // value to bitwise and with accumulator
    cpu.A = 0xFF78;

    TestInstruction(cpu, 10, 0x00001026);

    EXPECT_EQ(cpu.A, 0xFF34 & 0xFF78) << "Accumulator should be bitwise & with value";
    EXPECT_EQ(cpu.X, 0x0002) << "X register should not be altered";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flag should be set";
    TestUnchangedState(cpu, Y, SP);
}


// AND INDIRECT YINDEXED TESTS
TEST_F(ANDTest, And_IndirectYIndexed_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_AND_IND_Y, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1234); // zp address of partial address of value to bitwise and with accumulator
    cpu.writeWord(0x00001234, 0x00012345); // partial address of value to bitwise and with accumulator
    cpu.Y = 0x0002;
    cpu.writeTwoBytes(0x00012347, 0x1234); // value to bitwise and with accumulator
    cpu.A = 0x5678;

    TestInstruction(cpu, 9, 0x00001026);

    EXPECT_EQ(cpu.A, 0x1234 & 0x5678) << "Accumulator should be bitwise & with value";
    EXPECT_EQ(cpu.Y, 0x0002) << "Y register should not be altered";
    TestUnchangedState(cpu, X, SP, P);
}

TEST_F(ANDTest, And_IndirectYIndexed_PAGECROSS) {
    LoadInstruction(cpu, AlienCPU::INS_AND_IND_Y, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1234); // zp address of partial address of value to bitwise and with accumulator
    cpu.writeWord(0x00001234, 0x00000001); // partial address of value to bitwise and with accumulator
    cpu.Y = 0xFFFF;
    cpu.writeTwoBytes(0x00010000, 0x1234); // value to bitwise and with accumulator
    cpu.A = 0x5678;

    TestInstruction(cpu, 11, 0x00001026);

    EXPECT_EQ(cpu.A, 0x1234 & 0x5678) << "Accumulator should be bitwise & with value";
    EXPECT_EQ(cpu.Y, 0xFFFF) << "Y register should not be altered";
    TestUnchangedState(cpu, X, SP, P);
}

TEST_F(ANDTest, And_IndirectYIndexed_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_AND_IND_Y, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1234); // zp address of partial address of value to bitwise and with accumulator
    cpu.writeWord(0x00001234, 0x00012345); // partial address of value to bitwise and with accumulator
    cpu.Y = 0x0002;
    cpu.writeTwoBytes(0x00012347, 0x0000); // value to bitwise and with accumulator
    cpu.A = 0x5678;

    TestInstruction(cpu, 9, 0x00001026);

    EXPECT_EQ(cpu.A, 0x0000 & 0x5678) << "Accumulator should be bitwise & with value";
    EXPECT_EQ(cpu.Y, 0x0002) << "Y register should not be altered";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flag should be set";
    TestUnchangedState(cpu, X, SP);
}

TEST_F(ANDTest, And_IndirectYIndexed_NEGATIVEFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_AND_IND_Y, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1234); // zp address of partial address of value to bitwise and with accumulator
    cpu.writeWord(0x00001234, 0x00012345); // partial address of value to bitwise and with accumulator
    cpu.Y = 0x0002;
    cpu.writeTwoBytes(0x00012347, 0xFF34); // value to bitwise and with accumulator
    cpu.A = 0xFF78;

    TestInstruction(cpu, 9, 0x00001026);

    EXPECT_EQ(cpu.A, 0xFF34 & 0xFF78) << "Accumulator should be bitwise & with value";
    EXPECT_EQ(cpu.Y, 0x0002) << "Y register should not be altered";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flag should be set";
    TestUnchangedState(cpu, X, SP);
}


// AND ZEROPAGE TESTS
TEST_F(ANDTest, And_ZeroPage_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_AND_ZP, 0x00011023);
    cpu.writeTwoBytes(0x00011024, 0x1234); // zp address of value to bitwise and with accumulator
    cpu.writeTwoBytes(0x00001234, 0x1234); // value to bitwise and with accumulator
    cpu.A = 0x5678;

    TestInstruction(cpu, 5, 0x00011026);

    EXPECT_EQ(cpu.A, 0x1234 & 0x5678) << "Accumulator should be bitwise & with value";
    TestUnchangedState(cpu, X, Y, SP, P);
}

TEST_F(ANDTest, And_ZeroPage_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_AND_ZP, 0x00011023);
    cpu.writeTwoBytes(0x00011024, 0x1234); // zp address of value to bitwise and with accumulator
    cpu.writeTwoBytes(0x00001234, 0x0000); // value to bitwise and with accumulator
    cpu.A = 0x5678;

    TestInstruction(cpu, 5, 0x00011026);

    EXPECT_EQ(cpu.A, 0x0000 & 0x5678) << "Accumulator should be bitwise & with value";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flag should be set";
    TestUnchangedState(cpu, X, Y, SP);
}

TEST_F(ANDTest, And_ZeroPage_NEGATIVEFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_AND_ZP, 0x00011023);
    cpu.writeTwoBytes(0x00011024, 0x1234); // zp address of value to bitwise and with accumulator
    cpu.writeTwoBytes(0x00001234, 0xFF34); // value to bitwise and with accumulator
    cpu.A = 0xFF78;

    TestInstruction(cpu, 5, 0x00011026);

    EXPECT_EQ(cpu.A, 0xFF34 & 0xFF78) << "Accumulator should be bitwise & with value";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flag should be set";
    TestUnchangedState(cpu, X, Y, SP);
}


// AND ZEROPAGE XINDEXED TESTS
TEST_F(ANDTest, And_ZeroPageXIndexed_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_AND_ZP_X, 0x00011023);
    cpu.writeTwoBytes(0x00011024, 0x1234); // partial zp address of value to bitwise and with accumulator
    cpu.X = 0x0002;
    cpu.writeTwoBytes(0x00001236, 0x1234); // value to bitwise and with accumulator
    cpu.A = 0x5678;

    TestInstruction(cpu, 6, 0x00011026);

    EXPECT_EQ(cpu.A, 0x1234 & 0x5678) << "Accumulator should be bitwise & with value";
    EXPECT_EQ(cpu.X, 0x0002) << "X register should not be altered";
    TestUnchangedState(cpu, Y, SP, P);
}

TEST_F(ANDTest, And_ZeroPageXIndexed_WRAPAROUND) {
    LoadInstruction(cpu, AlienCPU::INS_AND_ZP_X, 0x00011023);
    cpu.writeTwoBytes(0x00011024, 0x0002); // partial zp address of value to bitwise and with accumulator
    cpu.X = 0xFFFF;
    cpu.writeTwoBytes(0x00000001, 0x1234); // value to bitwise and with accumulator
    cpu.A = 0x5678;

    TestInstruction(cpu, 6, 0x00011026);

    EXPECT_EQ(cpu.A, 0x1234 & 0x5678) << "Accumulator should be bitwise & with value";
    EXPECT_EQ(cpu.X, 0xFFFF) << "X register should not be altered";
    TestUnchangedState(cpu, Y, SP, P);
}

TEST_F(ANDTest, And_ZeroPageXIndexed_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_AND_ZP_X, 0x00011023);
    cpu.writeTwoBytes(0x00011024, 0x1234); // partial zp address of value to bitwise and with accumulator
    cpu.X = 0x0002;
    cpu.writeTwoBytes(0x00001236, 0x0000); // value to bitwise and with accumulator
    cpu.A = 0x5678;

    TestInstruction(cpu, 6, 0x00011026);

    EXPECT_EQ(cpu.A, 0x0000 & 0x5678) << "Accumulator should be bitwise & with value";
    EXPECT_EQ(cpu.X, 0x0002) << "X register should not be altered";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flag should be set";
    TestUnchangedState(cpu, Y, SP);
}

TEST_F(ANDTest, And_ZeroPageXIndexed_NEGATIVEFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_AND_ZP_X, 0x00011023);
    cpu.writeTwoBytes(0x00011024, 0x1234); // partial zp address of value to bitwise and with accumulator
    cpu.X = 0x0002; 
    cpu.writeTwoBytes(0x00001236, 0xFF34); // value to bitwise and with accumulator
    cpu.A = 0xFF78;

    TestInstruction(cpu, 6, 0x00011026);

    EXPECT_EQ(cpu.A, 0xFF34 & 0xFF78) << "Accumulator should be bitwise & with value";
    EXPECT_EQ(cpu.X, 0x0002) << "X register should not be altered";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flag should be set";
    TestUnchangedState(cpu, Y, SP);
}
