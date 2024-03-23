#include <emulator6502_tests\AlienCPUTest.h>

class LDYTest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};


// LDX IMMEDIATE TESTS
TEST_F(LDYTest, LoadY_Immediate_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_LDY_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x4232); // value to load into Y

    TestInstruction(cpu, 3, 0x00001026);

    EXPECT_EQ(cpu.Y, 0x4232) << "Y register is not set to the correct value";
    TestUnchangedState(cpu, A, X, SP, P);
}

TEST_F(LDYTest, LoadY_Immediate_ZFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LDY_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x0000); // value to load into Y

    TestInstruction(cpu, 3, 0x00001026);

    EXPECT_EQ(cpu.Y, 0x0000) << "Y register is not set to the correct value";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flags should be set";
    TestUnchangedState(cpu, A, X, SP);
}

TEST_F(LDYTest, LoadY_Immediate_NFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LDY_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0xFFEF); // value to load into Y

    TestInstruction(cpu, 3, 0x00001026);

    EXPECT_EQ(cpu.Y, 0xFFEF) << "Y register is not set to the correct value";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flags should be set";
    TestUnchangedState(cpu, A, X, SP);
}


// LDX ABSOLUTE TESTS
TEST_F(LDYTest, LoadY_Absolute_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_LDY_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00014232); // address of value to load into Y
    cpu.writeTwoBytes(0x00014232, 0x1234); // value to load into Y

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.Y, 0x1234) << "Y register is not set to the correct value";
    TestUnchangedState(cpu, A, X, SP, P);
}

TEST_F(LDYTest, LoadY_Absolute_ZFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LDY_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00014232); // address of value to load into Y
    cpu.writeTwoBytes(0x00014232, 0x0000); // value to load into Y

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.Y, 0x0000) << "Y register is not set to the correct value";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flags should be set";
    TestUnchangedState(cpu, A, X, SP);
}

TEST_F(LDYTest, LoadY_Absolute_NFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LDY_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00014232); // address of value to load into Y
    cpu.writeTwoBytes(0x00014232, 0xFFEF); // value to load into Y

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.Y, 0xFFEF) << "Y register is not set to the correct value";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flags should be set";
    TestUnchangedState(cpu, A, X, SP);
}


// LDX ABSOLUTE Y-INDEXED TESTS
TEST_F(LDYTest, LoadY_Absolute_XIndexed_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_LDY_ABS_X, 0x00001023);
    cpu.writeWord(0x00001024, 0x00014232); // partial address of value to load into Y
    cpu.X = 0x0013;
    cpu.writeTwoBytes(0x00014245, 0x1234); // value to load into Y

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.Y, 0x1234) << "Y register is not set to the correct value";
    EXPECT_EQ(cpu.X, 0x0013) << "X register should not be altered";
    TestUnchangedState(cpu, A, SP, P);
}

TEST_F(LDYTest, LoadY_Absolute_XIndexed_PAGECROSS) {
    LoadInstruction(cpu, AlienCPU::INS_LDY_ABS_X, 0x00001023);
    cpu.writeWord(0x00001024, 0x00011232); // partial address of value to load into Y
    cpu.X = 0xF013;
    cpu.writeTwoBytes(0x00020245, 0x1234); // value to load into Y

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.Y, 0x1234) << "Y register is not set to the correct value";
    EXPECT_EQ(cpu.X, 0xF013) << "X register should not be altered";
    TestUnchangedState(cpu, A, SP, P);
}

TEST_F(LDYTest, LoadY_Absolute_XIndexed_ZFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LDY_ABS_X, 0x00001023);
    cpu.writeWord(0x00001024, 0x00014232); // partial address of value to load into Y
    cpu.X = 0x0013;
    cpu.writeTwoBytes(0x00014245, 0x0000); // value to load into Y

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.Y, 0x0000) << "Y register is not set to the correct value";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flags should be set";
    EXPECT_EQ(cpu.X, 0x0013) << "X register should not be altered";
    TestUnchangedState(cpu, A, SP);
}

TEST_F(LDYTest, LoadY_Absolute_XIndexed_NFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LDY_ABS_X, 0x00001023);
    cpu.writeWord(0x00001024, 0x00014232); // partial address of value to load into Y
    cpu.X = 0x0013;
    cpu.writeTwoBytes(0x00014245, 0xFFEF); // value to load into Y

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.Y, 0xFFEF) << "Y register is not set to the correct value";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flags should be set";
    EXPECT_EQ(cpu.X, 0x0013) << "X register should not be altered";
    TestUnchangedState(cpu, A, SP);
}


// LDX ZEROPAGE TESTS
TEST_F(LDYTest, LoadY_ZeroPage_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_LDY_ZP, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x4232); // zp address to the value to load into Y
    cpu.writeTwoBytes(0x00004232, 0x2042); // value to load into Y

    TestInstruction(cpu, 5, 0x00001026);

    EXPECT_EQ(cpu.Y, 0x2042) << "Y register is not set to the correct value";
    TestUnchangedState(cpu, A, X, SP, P);
}

TEST_F(LDYTest, LoadY_ZeroPage_ZFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LDY_ZP, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x4232); // zp address to the value to load into Y
    cpu.writeTwoBytes(0x00004232, 0x0000); // value to load into Y

    TestInstruction(cpu, 5, 0x00001026);

    EXPECT_EQ(cpu.Y, 0x0000) << "Y register is not set to the correct value";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flags should be set";
    TestUnchangedState(cpu, A, X, SP);
}

TEST_F(LDYTest, LoadY_ZeroPage_NFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LDY_ZP, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x4232); // zp address to the value to load into Y
    cpu.writeTwoBytes(0x00004232, 0xFFEF); // value to load into Y

    TestInstruction(cpu, 5, 0x00001026);

    EXPECT_EQ(cpu.Y, 0xFFEF) << "Y register is not set to the correct value";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flags should be set";
    TestUnchangedState(cpu, A, X, SP);
}


// LDX ZEROPAGE X-INDEXED TESTS
TEST_F(LDYTest, LoadY_ZeroPage_XIndexed_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_LDY_ZP_X, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x4232); // partial zp address to the value to load into Y
    cpu.X = 0x0013;
    cpu.writeTwoBytes(0x00004245, 0x2042); // value to load into Y

    TestInstruction(cpu, 6, 0x00001026);

    EXPECT_EQ(cpu.Y, 0x2042) << "Y register is not set to the correct value";
    EXPECT_EQ(cpu.X, 0x0013) << "X register should not be altered";
    TestUnchangedState(cpu, A, SP, P);
}

TEST_F(LDYTest, LoadY_ZeroPage_XIndexed_WRAPAROUND) {
    LoadInstruction(cpu, AlienCPU::INS_LDY_ZP_X, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1232); // partial zp address to the value to load into Y
    cpu.X = 0xF013;
    cpu.writeTwoBytes(0x00000245, 0x2042); // value to load into Y

    TestInstruction(cpu, 6, 0x00001026);

    EXPECT_EQ(cpu.Y, 0x2042) << "Y register is not set to the correct value";
    EXPECT_EQ(cpu.X, 0xF013) << "X register should not be altered";
    TestUnchangedState(cpu, A, SP, P);
}

TEST_F(LDYTest, LoadY_ZeroPage_XIndexed_ZFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LDY_ZP_X, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x4232); // partial zp address to the value to load into Y
    cpu.X = 0x0013;
    cpu.writeTwoBytes(0x00004245, 0x0000); // value to load into Y

    TestInstruction(cpu, 6, 0x00001026);

    EXPECT_EQ(cpu.Y, 0x0000) << "Y register is not set to the correct value";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flags should be set";
    EXPECT_EQ(cpu.X, 0x0013) << "X register should not be altered";
    TestUnchangedState(cpu, A, SP);
}

TEST_F(LDYTest, LoadY_ZeroPage_XIndexed_NFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LDY_ZP_X, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x4232); // partial zp address to the value to load into Y
    cpu.X = 0x0013;
    cpu.writeTwoBytes(0x00004245, 0xFFEF); // value to load into Y

    TestInstruction(cpu, 6, 0x00001026);

    EXPECT_EQ(cpu.Y, 0xFFEF) << "Y register is not set to the correct value";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flags should be set";
    EXPECT_EQ(cpu.X, 0x0013) << "X register should not be altered";
    TestUnchangedState(cpu, A, SP);
}