#include <emulator6502_tests\AlienCPUTest.h>

class LDATest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};


// LDA IMMEDIATE TESTS
TEST_F(LDATest, LoadAccumulator_Immediate_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_LDA_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x4232); // value to load into accumulator

    TestInstruction(cpu, 3, 0x00001026);

    EXPECT_EQ(cpu.A, 0x4232) << "Accumulator is not set to the correct value";
    TestUnchangedState(cpu, X, Y, SP, P);
}

TEST_F(LDATest, LoadAccumulator_Immediate_ZFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LDA_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x0000); // value to load into accumulator

    TestInstruction(cpu, 3, 0x00001026);

    EXPECT_EQ(cpu.A, 0x0000) << "Accumulator is not set to the correct value";
    EXPECT_EQ(cpu.P, 0b00100010)<< "Only default and zero flag should be set";
    TestUnchangedState(cpu, X, Y, SP);
}

TEST_F(LDATest, LoadAccumulator_Immediate_NFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LDA_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0xFFEF); // value to load into accumulator

    TestInstruction(cpu, 3, 0x00001026);

    EXPECT_EQ(cpu.A, 0xFFEF) << "Accumulator is not set to the correct value";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flags should be set";
    TestUnchangedState(cpu, X, Y, SP);
}


// LDA ABSOLUTE TESTS
TEST_F(LDATest, LoadAccumulator_Absolute_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_LDA_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00014232); // address of value to load into accumulator
    cpu.writeTwoBytes(0x00014232, 0x1234); // value to load into accumulator

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.A, 0x1234) << "Accumulator is not set to the correct value";
    TestUnchangedState(cpu, X, Y, SP, P);
}

TEST_F(LDATest, LoadAccumulator_Absolute_ZFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LDA_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00014232); // address of value to load into accumulator
    cpu.writeTwoBytes(0x00014232, 0x0000); // value to load into accumulator

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.A, 0x0000) << "Accumulator is not set to the correct value";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flags should be set";
    TestUnchangedState(cpu, X, Y, SP);
}

TEST_F(LDATest, LoadAccumulator_Absolute_NFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LDA_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00014232); // address of value to load into accumulator
    cpu.writeTwoBytes(0x00014232, 0xFFEF); // value to load into accumulator

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.A, 0xFFEF) << "Accumulator is not set to the correct value";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flags should be set";
    TestUnchangedState(cpu, X, Y, SP);
}


// LDA ABSOLUTE X-INDEXED TESTS
TEST_F(LDATest, LoadAccumulator_Absolute_XIndexed_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_LDA_ABS_X, 0x00001023);
    cpu.writeWord(0x00001024, 0x00014232); // partial address of value to load into accumulator
    cpu.X = 0x0013;
    cpu.writeTwoBytes(0x00014245, 0x1234); // value to load into accumulator

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.A, 0x1234) << "Accumulator is not set to the correct value";
    EXPECT_EQ(cpu.X, 0x0013) << "X register should not be altered";
    TestUnchangedState(cpu, Y, SP, P);
}

TEST_F(LDATest, LoadAccumulator_Absolute_XIndexed_PAGECROSS) {
    LoadInstruction(cpu, AlienCPU::INS_LDA_ABS_X, 0x00001023);
    cpu.writeWord(0x00001024, 0x00011232); // partial address of value to load into accumulator
    cpu.X = 0xF013;
    cpu.writeTwoBytes(0x00020245, 0x1234); // value to load into accumulator

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.A, 0x1234) << "Accumulator is not set to the correct value";
    EXPECT_EQ(cpu.X, 0xF013) << "X register should not be altered";
    TestUnchangedState(cpu, Y, SP, P);
}

TEST_F(LDATest, LoadAccumulator_Absolute_XIndexed_ZFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LDA_ABS_X, 0x00001023);
    cpu.writeWord(0x00001024, 0x00014232); // partial address of value to load into accumulator
    cpu.X = 0x0013;
    cpu.writeTwoBytes(0x00014245, 0x0000); // value to load into accumulator

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.A, 0x0000) << "Accumulator is not set to the correct value";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flags should be set";
    EXPECT_EQ(cpu.X, 0x0013) << "X register should not be altered";
    TestUnchangedState(cpu, Y, SP);
}

TEST_F(LDATest, LoadAccumulator_Absolute_XIndexed_NFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LDA_ABS_X, 0x00001023);
    cpu.writeWord(0x00001024, 0x00014232); // partial address of value to load into accumulator
    cpu.X = 0x0013;
    cpu.writeTwoBytes(0x00014245, 0xFFEF); // value to load into accumulator

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.A, 0xFFEF) << "Accumulator is not set to the correct value";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flags should be set";
    EXPECT_EQ(cpu.X, 0x0013) << "X register should not be altered";
    TestUnchangedState(cpu, Y, SP);
}


// LDA ABSOLUTE Y-INDEXED TESTS
TEST_F(LDATest, LoadAccumulator_Absolute_YIndexed_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_LDA_ABS_Y, 0x00001023);
    cpu.writeWord(0x00001024, 0x00014232); // partial address of value to load into accumulator
    cpu.Y = 0x0013;
    cpu.writeTwoBytes(0x00014245, 0x1234); // value to load into accumulator

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.A, 0x1234) << "Accumulator is not set to the correct value";
    EXPECT_EQ(cpu.Y, 0x0013) << "Y register should not be altered";
    TestUnchangedState(cpu, X, SP, P);
}

TEST_F(LDATest, LoadAccumulator_Absolute_YIndexed_PAGECROSS) {
    LoadInstruction(cpu, AlienCPU::INS_LDA_ABS_Y, 0x00001023);
    cpu.writeWord(0x00001024, 0x00011232); // partial address of value to load into accumulator
    cpu.Y = 0xF013;
    cpu.writeTwoBytes(0x00020245, 0x1234); // value to load into accumulator

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.A, 0x1234) << "Accumulator is not set to the correct value";
    EXPECT_EQ(cpu.Y, 0xF013) << "Y register should not be altered";
    TestUnchangedState(cpu, X, SP, P);
}

TEST_F(LDATest, LoadAccumulator_Absolute_YIndexed_ZFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LDA_ABS_Y, 0x00001023);
    cpu.writeWord(0x00001024, 0x00014232); // partial address of value to load into accumulator
    cpu.Y = 0x0013;
    cpu.writeTwoBytes(0x00014245, 0x0000); // value to load into accumulator

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.A, 0x0000) << "Accumulator is not set to the correct value";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flags should be set";
    EXPECT_EQ(cpu.Y, 0x0013) << "Y register should not be altered";
    TestUnchangedState(cpu, X, SP);
}

TEST_F(LDATest, LoadAccumulator_Absolute_YIndexed_NFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LDA_ABS_Y, 0x00001023);
    cpu.writeWord(0x00001024, 0x00014232); // partial address of value to load into accumulator
    cpu.Y = 0x0013;
    cpu.writeTwoBytes(0x00014245, 0xFFEF); // value to load into accumulator

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.A, 0xFFEF) << "Accumulator is not set to the correct value";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flags should be set";
    EXPECT_EQ(cpu.Y, 0x0013) << "Y register should not be altered";
    TestUnchangedState(cpu, X, SP);
}


// LDA ZEROPAGE TESTS
TEST_F(LDATest, LoadAccumulator_ZeroPage_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_LDA_ZP, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x4232); // zp address to the value to load into accumulator
    cpu.writeTwoBytes(0x00004232, 0x2042); // value to load into accumulator

    TestInstruction(cpu, 5, 0x00001026);

    EXPECT_EQ(cpu.A, 0x2042) << "Accumulator is not set to the correct value";
    TestUnchangedState(cpu, X, Y, SP, P);
}

TEST_F(LDATest, LoadAccumulator_ZeroPage_ZFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LDA_ZP, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x4232); // zp address to the value to load into accumulator
    cpu.writeTwoBytes(0x00004232, 0x0000); // value to load into accumulator

    TestInstruction(cpu, 5, 0x00001026);

    EXPECT_EQ(cpu.A, 0x0000) << "Accumulator is not set to the correct value";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flags should be set";
    TestUnchangedState(cpu, X, Y, SP);
}

TEST_F(LDATest, LoadAccumulator_ZeroPage_NFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LDA_ZP, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x4232); // zp address to the value to load into accumulator
    cpu.writeTwoBytes(0x00004232, 0xFFEF); // value to load into accumulator

    TestInstruction(cpu, 5, 0x00001026);

    EXPECT_EQ(cpu.A, 0xFFEF) << "Accumulator is not set to the correct value";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flags should be set";
    TestUnchangedState(cpu, X, Y, SP);
}


// LDA ZEROPAGE X-INDEXED TESTS
TEST_F(LDATest, LoadAccumulator_ZeroPage_XIndexed_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_LDA_ZP_X, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x4232); // partial zp address to the value to load into accumulator
    cpu.X = 0x0013;
    cpu.writeTwoBytes(0x00004245, 0x2042); // value to load into accumulator

    TestInstruction(cpu, 6, 0x00001026);

    EXPECT_EQ(cpu.A, 0x2042) << "Accumulator is not set to the correct value";
    EXPECT_EQ(cpu.X, 0x0013) << "X register should not be altered";
    TestUnchangedState(cpu, Y, SP, P);
}

TEST_F(LDATest, LoadAccumulator_ZeroPage_XIndexed_WRAPAROUND) {
    LoadInstruction(cpu, AlienCPU::INS_LDA_ZP_X, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1232); // partial zp address to the value to load into accumulator
    cpu.X = 0xF013;
    cpu.writeTwoBytes(0x00000245, 0x2042); // value to load into accumulator

    TestInstruction(cpu, 6, 0x00001026);

    EXPECT_EQ(cpu.A, 0x2042) << "Accumulator is not set to the correct value";
    EXPECT_EQ(cpu.X, 0xF013) << "X register should not be altered";
    TestUnchangedState(cpu, Y, SP, P);
}

TEST_F(LDATest, LoadAccumulator_ZeroPage_XIndexed_ZFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LDA_ZP_X, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x4232); // partial zp address to the value to load into accumulator
    cpu.X = 0x0013;
    cpu.writeTwoBytes(0x00004245, 0x0000); // value to load into accumulator

    TestInstruction(cpu, 6, 0x00001026);

    EXPECT_EQ(cpu.A, 0x0000) << "Accumulator is not set to the correct value";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flags should be set";
    EXPECT_EQ(cpu.X, 0x0013) << "X register should not be altered";
    TestUnchangedState(cpu, Y, SP);
}

TEST_F(LDATest, LoadAccumulator_ZeroPage_XIndexed_NFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LDA_ZP_X, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x4232); // partial zp address to the value to load into accumulator
    cpu.X = 0x0013;
    cpu.writeTwoBytes(0x00004245, 0xFFEF); // value to load into accumulator

    TestInstruction(cpu, 6, 0x00001026);

    EXPECT_EQ(cpu.A, 0xFFEF) << "Accumulator is not set to the correct value";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flags should be set";
    EXPECT_EQ(cpu.X, 0x0013) << "X register should not be altered";
    TestUnchangedState(cpu, Y, SP);
}


// LDA X-INDEXED INDIRECT TESTS
TEST_F(LDATest, LoadAccumulator_XIndexed_Indirect_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_LDA_X_IND, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x4232); // partial zp address of address of value to load into accumulator
    cpu.X = 0x0013;
    cpu.writeWord(0x00004245, 0x00011234); // address of value to load into accumulator
    cpu.writeTwoBytes(0x00011234, 0x1234); // value to load into accumulator

    TestInstruction(cpu, 10, 0x00001026);
    
    EXPECT_EQ(cpu.A, 0x1234) << "Accumulator is not set to the correct value";
    EXPECT_EQ(cpu.X, 0x0013) << "X register should not be altered";
    TestUnchangedState(cpu, Y, SP, P);
}

TEST_F(LDATest, LoadAccumulator_XIndexed_Indirect_WRAPAROUND) {
    LoadInstruction(cpu, AlienCPU::INS_LDA_X_IND, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1232); // partial zp address of address of value to load into accumulator
    cpu.X = 0xF013;
    cpu.writeWord(0x00000245, 0x00011234); // address of value to load into accumulator
    cpu.writeTwoBytes(0x00011234, 0x1234); // value to load into accumulator

    TestInstruction(cpu, 10, 0x00001026);

    EXPECT_EQ(cpu.A, 0x1234) << "Accumulator is not set to the correct value";
    EXPECT_EQ(cpu.X, 0xF013) << "X register should not be altered";
    TestUnchangedState(cpu, Y, SP, P);
}

TEST_F(LDATest, LoadAccumulator_XIndexed_Indirect_ZFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LDA_X_IND, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x4232); // partial zp address of address of value to load into accumulator
    cpu.X = 0x0013;
    cpu.writeWord(0x00004245, 0x00011234); // address of value to load into accumulator
    cpu.writeTwoBytes(0x00011234, 0x0000); // value to load into accumulator

    TestInstruction(cpu, 10, 0x00001026);

    EXPECT_EQ(cpu.A, 0x0000) << "Accumulator is not set to the correct value";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flags should be set";
    EXPECT_EQ(cpu.X, 0x0013) << "X register should not be altered";
    TestUnchangedState(cpu, Y, SP);
}

TEST_F(LDATest, LoadAccumulator_XIndexed_Indirect_NFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LDA_X_IND, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x4232); // partial zp address of address of value to load into accumulator
    cpu.X = 0x0013;
    cpu.writeWord(0x00004245, 0x00011234); // address of value to load into accumulator
    cpu.writeTwoBytes(0x00011234, 0xFFEF); // value to load into accumulator

    TestInstruction(cpu, 10, 0x00001026);

    EXPECT_EQ(cpu.A, 0xFFEF) << "Accumulator is not set to the correct value";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flags should be set";
    EXPECT_EQ(cpu.X, 0x0013) << "X register should not be altered";
    TestUnchangedState(cpu, Y, SP);
}


// LDA INDIRECT Y-INDEXED TESTS
TEST_F(LDATest, LoadAccumulator_Indirect_YIndexed_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_LDA_IND_Y, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x4232); // zp address of partial address of value to load into accumulator
    cpu.writeWord(0x00004232, 0x00011234); // partial address of value to load into accumulator
    cpu.Y = 0x0013;
    cpu.writeTwoBytes(0x00011247, 0x1234); // value to load into accumulator

    TestInstruction(cpu, 10, 0x00001026);

    EXPECT_EQ(cpu.A, 0x1234) << "Accumulator is not set to the correct value";
    EXPECT_EQ(cpu.Y, 0x0013) << "Y register should not be altered";
    TestUnchangedState(cpu, X, SP, P);
}

TEST_F(LDATest, LoadAccumulator_Indirect_YIndexed_PAGECROSS) {
    LoadInstruction(cpu, AlienCPU::INS_LDA_IND_Y, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x4232); // zp address of partial address to load into accumulator
    cpu.writeWord(0x00004232, 0x00011234); // partial address of value to load into accumulator
    cpu.Y = 0xF013;
    cpu.writeTwoBytes(0x00020247, 0x1234); // value to load into accumulator

    TestInstruction(cpu, 10, 0x00001026);

    EXPECT_EQ(cpu.A, 0x1234) << "Accumulator is not set to the correct value";
    EXPECT_EQ(cpu.Y, 0xF013) << "Y register should not be altered";
    TestUnchangedState(cpu, X, SP, P);
}

TEST_F(LDATest, LoadAccumulator_Indirect_YIndexed_ZFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LDA_IND_Y, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x4232); // zp address of partial address to load into accumulator
    cpu.writeWord(0x00004232, 0x00011234); // partial address of value to load into accumulator
    cpu.Y = 0x0013;
    cpu.writeTwoBytes(0x00011247, 0x0000); // value to load into accumulator

    TestInstruction(cpu, 10, 0x00001026);

    EXPECT_EQ(cpu.A, 0x0000) << "Accumulator is not set to the correct value";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default flag should be set";
    EXPECT_EQ(cpu.Y, 0x0013) << "Y register should not be altered";
    TestUnchangedState(cpu, X, SP);
}

TEST_F(LDATest, LoadAccumulator_Indirect_YIndexed_NFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LDA_IND_Y, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x4232); // zp address of partial address to load into accumulator
    cpu.writeWord(0x00004232, 0x00011234); // partial address of value to load into accumulator
    cpu.Y = 0x0013;
    cpu.writeTwoBytes(0x00011247, 0xFFEF); // value to load into accumulator

    TestInstruction(cpu, 10, 0x00001026);
    
    EXPECT_EQ(cpu.A, 0xFFEF) << "Accumulator is not set to the correct value";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default flag should be set";
    EXPECT_EQ(cpu.Y, 0x0013) << "Y register should not be altered";
    TestUnchangedState(cpu, X, SP);
}