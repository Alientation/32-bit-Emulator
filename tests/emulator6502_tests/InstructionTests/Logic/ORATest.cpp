#include <emulator6502_tests\AlienCPUTest.h>

class ORATest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};


// ORA IMMEDIATE TESTS
TEST_F(ORATest, InclusiveOr_Immediate_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_ORA_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1234); // value to bitwise inclusive or with accumulator
    cpu.A = 0x5678;

    TestInstruction(cpu, 3, 0x00001026);

    EXPECT_EQ(cpu.A, 0x1234 | 0x5678) << "Accumulator should be bitwise inclusive or with value";
    TestUnchangedState(cpu, X, Y, SP, P);
}

TEST_F(ORATest, InclusiveOr_Immediate_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ORA_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x0000); // value to bitwise inclusive or with accumulator
    cpu.A = 0x0000;

    TestInstruction(cpu, 3, 0x00001026);

    EXPECT_EQ(cpu.A, 0x0000 | 0x0000) << "Accumulator should be bitwise inclusive or with value";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flag should be set";
    TestUnchangedState(cpu, X, Y, SP);
}

TEST_F(ORATest, InclusiveOr_Immediate_NEGATIVEFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ORA_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0xFF34); // value to bitwise inclusive or with accumulator
    cpu.A = 0xFF78;

    TestInstruction(cpu, 3, 0x00001026);

    EXPECT_EQ(cpu.A, 0xFF34 | 0xFF78) << "Accumulator should be bitwise inclusive or with value";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flag should be set";
    TestUnchangedState(cpu, X, Y, SP);
}


// ORA ABSOLUTE TESTS
TEST_F(ORATest, InclusiveOr_Absolute_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_ORA_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012345); // address of value to bitwise inclusive or with accumulator
    cpu.writeTwoBytes(0x00012345, 0x1234); // value to bitwise inclusive or with accumulator
    cpu.A = 0x5678;

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.A, 0x1234 | 0x5678) << "Accumulator should be bitwise inclusive or with value";
    TestUnchangedState(cpu, X, Y, SP, P);
}

TEST_F(ORATest, InclusiveOr_Absolute_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ORA_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012345); // address of value to bitwise inclusive or with accumulator
    cpu.writeTwoBytes(0x00012345, 0x0000); // value to bitwise inclusive or with accumulator
    cpu.A = 0x0000;

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.A, 0x0000 | 0x0000) << "Accumulator should be bitwise inclusive or with value";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flag should be set";
    TestUnchangedState(cpu, X, Y, SP);
}

TEST_F(ORATest, InclusiveOr_Absolute_NEGATIVEFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ORA_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012345); // address of value to bitwise inclusive or with accumulator
    cpu.writeTwoBytes(0x00012345, 0xFF34); // value to bitwise inclusive or with accumulator
    cpu.A = 0xFF78;

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.A, 0xFF34 | 0xFF78) << "Accumulator should be bitwise inclusive or with value";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flag should be set";
    TestUnchangedState(cpu, X, Y, SP);
}


// ORA ABSOLUTE XINDEXED TESTS
TEST_F(ORATest, InclusiveOr_AbsoluteXIndexed_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_ORA_ABS_X, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012345); // partial address of value to bitwise inclusive or with accumulator
    cpu.X = 0x0002;
    cpu.writeTwoBytes(0x00012347, 0x1234); // value to bitwise inclusive or with accumulator
    cpu.A = 0x5678;

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.A, 0x1234 | 0x5678) << "Accumulator should be bitwise inclusive or with value";
    EXPECT_EQ(cpu.X, 0x0002) << "X should not be altered";
    TestUnchangedState(cpu, Y, SP, P);
}

TEST_F(ORATest, InclusiveOr_AbsoluteXIndexed_PAGECROSSING) {
    LoadInstruction(cpu, AlienCPU::INS_ORA_ABS_X, 0x00001023);
    cpu.writeWord(0x00001024, 0x00000001); // partial address of value to bitwise inclusive or with accumulator
    cpu.X = 0xFFFF;
    cpu.writeTwoBytes(0x00010000, 0x1234); // value to bitwise inclusive or with accumulator
    cpu.A = 0x5678;

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.A, 0x1234 | 0x5678) << "Accumulator should be bitwise inclusive or with value";
    EXPECT_EQ(cpu.X, 0xFFFF) << "X should not be altered";
    TestUnchangedState(cpu, Y, SP, P);
}

TEST_F(ORATest, InclusiveOr_AbsoluteXIndexed_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ORA_ABS_X, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012345); // partial address of value to bitwise inclusive or with accumulator
    cpu.X = 0x0002;
    cpu.writeTwoBytes(0x00012347, 0x0000); // value to bitwise inclusive or with accumulator
    cpu.A = 0x0000;

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.A, 0x0000 | 0x0000) << "Accumulator should be bitwise inclusive or with value";
    EXPECT_EQ(cpu.X, 0x0002) << "X should not be altered";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flag should be set";
    TestUnchangedState(cpu, Y, SP);
}

TEST_F(ORATest, InclusiveOr_AbsoluteXIndexed_NEGATIVEFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ORA_ABS_X, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012345); // partial address of value to bitwise inclusive or with accumulator
    cpu.X = 0x0002;
    cpu.writeTwoBytes(0x00012347, 0xFF34); // value to bitwise inclusive or with accumulator
    cpu.A = 0xFF78;

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.A, 0xFF34 | 0xFF78) << "Accumulator should be bitwise inclusive or with value";
    EXPECT_EQ(cpu.X, 0x0002) << "X should not be altered";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flag should be set";
    TestUnchangedState(cpu, Y, SP);
}


// ORA ABSOLUTE YINDEXED TESTS
TEST_F(ORATest, InclusiveOr_AbsoluteYIndexed_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_ORA_ABS_Y, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012345); // partial address of value to bitwise inclusive or with accumulator
    cpu.Y = 0x0002;
    cpu.writeTwoBytes(0x00012347, 0x1234); // value to bitwise inclusive or with accumulator
    cpu.A = 0x5678;

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.A, 0x1234 | 0x5678) << "Accumulator should be bitwise inclusive or with value";
    EXPECT_EQ(cpu.Y, 0x0002) << "Y should not be altered";
    TestUnchangedState(cpu, X, SP, P);
}

TEST_F(ORATest, InclusiveOr_AbsoluteYIndexed_PAGECROSSING) {
    LoadInstruction(cpu, AlienCPU::INS_ORA_ABS_Y, 0x00001023);
    cpu.writeWord(0x00001024, 0x00000001); // partial address of value to bitwise inclusive or with accumulator
    cpu.Y = 0xFFFF;
    cpu.writeTwoBytes(0x00010000, 0x1234); // value to bitwise inclusive or with accumulator
    cpu.A = 0x5678;

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.A, 0x1234 | 0x5678) << "Accumulator should be bitwise inclusive or with value";
    EXPECT_EQ(cpu.Y, 0xFFFF) << "Y should not be altered";
    TestUnchangedState(cpu, X, SP, P);
}

TEST_F(ORATest, InclusiveOr_AbsoluteYIndexed_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ORA_ABS_Y, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012345); // partial address of value to bitwise inclusive or with accumulator
    cpu.Y = 0x0002;
    cpu.writeTwoBytes(0x00012347, 0x0000); // value to bitwise inclusive or with accumulator
    cpu.A = 0x0000;

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.A, 0x0000 | 0x0000) << "Accumulator should be bitwise inclusive or with value";
    EXPECT_EQ(cpu.Y, 0x0002) << "Y should not be altered";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flag should be set";
    TestUnchangedState(cpu, X, SP);
}

TEST_F(ORATest, InclusiveOr_AbsoluteYIndexed_NEGATIVEFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ORA_ABS_Y, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012345); // partial address of value to bitwise inclusive or with accumulator
    cpu.Y = 0x0002;
    cpu.writeTwoBytes(0x00012347, 0xFF34); // value to bitwise inclusive or with accumulator
    cpu.A = 0xFF78;

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.A, 0xFF34 | 0xFF78) << "Accumulator should be bitwise inclusive or with value";
    EXPECT_EQ(cpu.Y, 0x0002) << "Y should not be altered";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flag should be set";
    TestUnchangedState(cpu, X, SP);
}


// ORA ZEROPAGE TESTS
TEST_F(ORATest, InclusiveOr_ZeroPage_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_ORA_ZP, 0x00011023);
    cpu.writeTwoBytes(0x00011024, 0x1234); // zp address of value to bitwise inclusive or with accumulator
    cpu.writeTwoBytes(0x00001234, 0x1234); // value to bitwise inclusive or with accumulator
    cpu.A = 0x5678;

    TestInstruction(cpu, 5, 0x00011026);

    EXPECT_EQ(cpu.A, 0x1234 | 0x5678) << "Accumulator should be bitwise inclusive or with value";
    TestUnchangedState(cpu, X, Y, SP, P);
}

TEST_F(ORATest, InclusiveOr_ZeroPage_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ORA_ZP, 0x00011023);
    cpu.writeTwoBytes(0x00011024, 0x1234); // zp address of value to bitwise inclusive or with accumulator
    cpu.writeTwoBytes(0x00001234, 0x0000); // value to bitwise inclusive or with accumulator
    cpu.A = 0x0000;

    TestInstruction(cpu, 5, 0x00011026);

    EXPECT_EQ(cpu.A, 0x0000 | 0x0000) << "Accumulator should be bitwise inclusive or with value";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flag should be set";
    TestUnchangedState(cpu, X, Y, SP);
}

TEST_F(ORATest, InclusiveOr_ZeroPage_NEGATIVEFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ORA_ZP, 0x00011023);
    cpu.writeTwoBytes(0x00011024, 0x1234); // zp address of value to bitwise inclusive or with accumulator
    cpu.writeTwoBytes(0x00001234, 0xFF34); // value to bitwise inclusive or with accumulator
    cpu.A = 0xFF78;

    TestInstruction(cpu, 5, 0x00011026);

    EXPECT_EQ(cpu.A, 0xFF34 | 0xFF78) << "Accumulator should be bitwise inclusive or with value";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flag should be set";
    TestUnchangedState(cpu, X, Y, SP);
}


// ORA ZEROPAGE XINDEXED TESTS
TEST_F(ORATest, InclusiveOr_ZeroPageXIndexed_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_ORA_ZP_X, 0x00011023);
    cpu.writeTwoBytes(0x00011024, 0x1234); // partial zp address of value to bitwise inclusive or with accumulator
    cpu.X = 0x0002;
    cpu.writeTwoBytes(0x00001236, 0x1234); // value to bitwise inclusive or with accumulator
    cpu.A = 0x5678;

    TestInstruction(cpu, 6, 0x00011026);

    EXPECT_EQ(cpu.A, 0x1234 | 0x5678) << "Accumulator should be bitwise inclusive or with value";
    EXPECT_EQ(cpu.X, 0x0002) << "X should not be altered";
    TestUnchangedState(cpu, Y, SP, P);
}

TEST_F(ORATest, InclusiveOr_ZeroPageXIndexed_WRAPAROUND) {
    LoadInstruction(cpu, AlienCPU::INS_ORA_ZP_X, 0x00011023);
    cpu.writeTwoBytes(0x00011024, 0x0002); // partial zp address of value to bitwise inclusive or with accumulator
    cpu.X = 0xFFFF;
    cpu.writeTwoBytes(0x00000001, 0x1234); // value to bitwise inclusive or with accumulator
    cpu.A = 0x5678;

    TestInstruction(cpu, 6, 0x00011026);

    EXPECT_EQ(cpu.A, 0x1234 | 0x5678) << "Accumulator should be bitwise inclusive or with value";
    EXPECT_EQ(cpu.X, 0xFFFF) << "X should not be altered";
    TestUnchangedState(cpu, Y, SP, P);
}

TEST_F(ORATest, InclusiveOr_ZeroPageXIndexed_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ORA_ZP_X, 0x00011023); 
    cpu.writeTwoBytes(0x00011024, 0x1234); // partial zp address of value to bitwise inclusive or with accumulator
    cpu.X = 0x0002;
    cpu.writeTwoBytes(0x00001236, 0x0000); // value to bitwise inclusive or with accumulator
    cpu.A = 0x0000;

    TestInstruction(cpu, 6, 0x00011026);

    EXPECT_EQ(cpu.A, 0x0000 | 0x0000) << "Accumulator should be bitwise inclusive or with value";
    EXPECT_EQ(cpu.X, 0x0002) << "X should not be altered";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flag should be set";
    TestUnchangedState(cpu, Y, SP);
}

TEST_F(ORATest, InclusiveOr_ZeroPageXIndexed_NEGATIVEFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ORA_ZP_X, 0x00011023);
    cpu.writeTwoBytes(0x00011024, 0x1234); // partial zp address of value to bitwise inclusive or with accumulator
    cpu.X = 0x0002;
    cpu.writeTwoBytes(0x00001236, 0xFF34); // value to bitwise inclusive or with accumulator
    cpu.A = 0xFF78;

    TestInstruction(cpu, 6, 0x00011026);

    EXPECT_EQ(cpu.A, 0xFF34 | 0xFF78) << "Accumulator should be bitwise inclusive or with value";
    EXPECT_EQ(cpu.X, 0x0002) << "X should not be altered";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flag should be set";
    TestUnchangedState(cpu, Y, SP);
}


// ORA XINDEXED INDIRECT TESTS
TEST_F(ORATest, InclusiveOr_XIndexedIndirect_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_ORA_X_IND, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1234); // partial zp address of address of value to bitwise inclusive or with accumulator
    cpu.X = 0x0002;
    cpu.writeWord(0x00001236, 0x00012345); // address of value to bitwise inclusive or with accumulator
    cpu.writeTwoBytes(0x00012345, 0x1234); // value to bitwise inclusive or with accumulator
    cpu.A = 0x5678;

    TestInstruction(cpu, 10, 0x00001026);

    EXPECT_EQ(cpu.A, 0x1234 | 0x5678) << "Accumulator should be bitwise inclusive or with value";
    EXPECT_EQ(cpu.X, 0x0002) << "X should not be altered";
    TestUnchangedState(cpu, Y, SP, P);
}

TEST_F(ORATest, InclusiveOr_XIndexedIndirect_WRAPAROUND) {
    LoadInstruction(cpu, AlienCPU::INS_ORA_X_IND, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1234); // partial zp address of address of value to bitwise inclusive or with accumulator
    cpu.X = 0xF002;
    cpu.writeWord(0x00000236, 0x00012345); // address of value to bitwise inclusive or with accumulator
    cpu.writeTwoBytes(0x00012345, 0x1234); // value to bitwise inclusive or with accumulator
    cpu.A = 0x5678;

    TestInstruction(cpu, 10, 0x00001026);

    EXPECT_EQ(cpu.A, 0x1234 | 0x5678) << "Accumulator should be bitwise inclusive or with value";
    EXPECT_EQ(cpu.X, 0xF002) << "X should not be altered";
    TestUnchangedState(cpu, Y, SP, P);
}

TEST_F(ORATest, InclusiveOr_XIndexedIndirect_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ORA_X_IND, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1234); // partial zp address of address of value to bitwise inclusive or with accumulator
    cpu.X = 0x0002;
    cpu.writeWord(0x00001236, 0x00012345); // address of value to bitwise inclusive or with accumulator
    cpu.writeTwoBytes(0x00012345, 0x0000); // value to bitwise inclusive or with accumulator
    cpu.A = 0x0000;

    TestInstruction(cpu, 10, 0x00001026);

    EXPECT_EQ(cpu.A, 0x0000 | 0x0000) << "Accumulator should be bitwise inclusive or with value";
    EXPECT_EQ(cpu.X, 0x0002) << "X should not be altered";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flag should be set";
    TestUnchangedState(cpu, Y, SP);
}

TEST_F(ORATest, InclusiveOr_XIndexedIndirect_NEGATIVEFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ORA_X_IND, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1234); // partial zp address of address of value to bitwise inclusive or with accumulator
    cpu.X = 0x0002;
    cpu.writeWord(0x00001236, 0x00012345); // address of value to bitwise inclusive or with accumulator
    cpu.writeTwoBytes(0x00012345, 0xFF34); // value to bitwise inclusive or with accumulator
    cpu.A = 0xFF78;

    TestInstruction(cpu, 10, 0x00001026);

    EXPECT_EQ(cpu.A, 0xFF34 | 0xFF78) << "Accumulator should be bitwise inclusive or with value";
    EXPECT_EQ(cpu.X, 0x0002) << "X should not be altered";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flag should be set";
    TestUnchangedState(cpu, Y, SP);
}


// ORA INDIRECT YINDEXED TESTS
TEST_F(ORATest, InclusiveOr_IndirectYIndexed_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_ORA_IND_Y, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1234); // zp address of partial address of value to bitwise inclusive or with accumulator
    cpu.writeWord(0x00001234, 0x00012345); // partial address of value to bitwise inclusive or with accumulator
    cpu.Y = 0x0002;
    cpu.writeTwoBytes(0x00012347, 0x1234); // value to bitwise inclusive or with accumulator
    cpu.A = 0x5678;

    TestInstruction(cpu, 10, 0x00001026);

    EXPECT_EQ(cpu.A, 0x1234 | 0x5678) << "Accumulator should be bitwise inclusive or with value";
    EXPECT_EQ(cpu.Y, 0x0002) << "Y should not be altered";
    TestUnchangedState(cpu, X, SP, P);
}

TEST_F(ORATest, InclusiveOr_IndirectYIndexed_PAGECROSSING) {
    LoadInstruction(cpu, AlienCPU::INS_ORA_IND_Y, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1234); // zp address of partial address of value to bitwise inclusive or with accumulator
    cpu.writeWord(0x00001234, 0x00000001); // partial address of value to bitwise inclusive or with accumulator
    cpu.Y = 0xFFFF;
    cpu.writeTwoBytes(0x00010000, 0x1234); // value to bitwise inclusive or with accumulator
    cpu.A = 0x5678;

    TestInstruction(cpu, 10, 0x00001026);

    EXPECT_EQ(cpu.A, 0x1234 | 0x5678) << "Accumulator should be bitwise inclusive or with value";
    EXPECT_EQ(cpu.Y, 0xFFFF) << "Y should not be altered";
    TestUnchangedState(cpu, X, SP, P);
}

TEST_F(ORATest, InclusiveOr_IndirectYIndexed_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ORA_IND_Y, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1234); // zp address of partial address of value to bitwise inclusive or with accumulator
    cpu.writeWord(0x00001234, 0x00012345); // partial address of value to bitwise inclusive or with accumulator
    cpu.Y = 0x0002;
    cpu.writeTwoBytes(0x00012347, 0x0000); // value to bitwise inclusive or with accumulator
    cpu.A = 0x0000;

    TestInstruction(cpu, 10, 0x00001026);

    EXPECT_EQ(cpu.A, 0x0000 | 0x0000) << "Accumulator should be bitwise inclusive or with value";
    EXPECT_EQ(cpu.Y, 0x0002) << "Y should not be altered";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flag should be set";
    TestUnchangedState(cpu, X, SP);
}

TEST_F(ORATest, InclusiveOr_IndirectYIndexed_NEGATIVEFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ORA_IND_Y, 0x00001023); 
    cpu.writeTwoBytes(0x00001024, 0x1234); // zp address of partial address of value to bitwise inclusive or with accumulator
    cpu.writeWord(0x00001234, 0x00012345); // partial address of value to bitwise inclusive or with accumulator
    cpu.Y = 0x0002;
    cpu.writeTwoBytes(0x00012347, 0xFF34); // value to bitwise inclusive or with accumulator
    cpu.A = 0xFF78;

    TestInstruction(cpu, 10, 0x00001026);

    EXPECT_EQ(cpu.A, 0xFF34 | 0xFF78) << "Accumulator should be bitwise inclusive or with value";
    EXPECT_EQ(cpu.Y, 0x0002) << "Y should not be altered";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flag should be set";
    TestUnchangedState(cpu, X, SP);
}