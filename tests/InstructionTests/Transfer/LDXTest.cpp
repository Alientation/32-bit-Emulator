#include <AlienCPUTest.h>

class LDXTest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};


// LDX IMMEDIATE TESTS
TEST_F(LDXTest, LoadX_Immediate_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_LDX_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x4232); // value to load into X
 
    TestInstruction(cpu, 3, 0x00001026);

    EXPECT_EQ(cpu.X, 0x4232) << "X register is not set to the correct value";
    TestUnchangedState(cpu, A, Y, SP, P);
}

TEST_F(LDXTest, LoadX_Immediate_ZFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LDX_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x0000); // value to load into X

    TestInstruction(cpu, 3, 0x00001026);
    
    EXPECT_EQ(cpu.X, 0x0000) << "X register is not set to the correct value";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flags should be set";
    TestUnchangedState(cpu, A, Y, SP);
}

TEST_F(LDXTest, LoadX_Immediate_NFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LDX_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0xFFEF); // value to load into X

    TestInstruction(cpu, 3, 0x00001026);

    EXPECT_EQ(cpu.X, 0xFFEF) << "X register is not set to the correct value";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flags should be set";
    TestUnchangedState(cpu, A, Y, SP);
}


// LDX ABSOLUTE TESTS
TEST_F(LDXTest, LoadX_Absolute_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_LDX_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00014232); // address of value to load into X
    cpu.writeTwoBytes(0x00014232, 0x1234); // value to load into X

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.X, 0x1234) << "X register is not set to the correct value";
    TestUnchangedState(cpu, A, Y, SP, P);
}

TEST_F(LDXTest, LoadX_Absolute_ZFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LDX_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00014232); // address of value to load into X
    cpu.writeTwoBytes(0x00014232, 0x0000); // value to load into X

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.X, 0x0000) << "X register is not set to the correct value";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flags should be set";
    TestUnchangedState(cpu, A, Y, SP);
}

TEST_F(LDXTest, LoadX_Absolute_NFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LDX_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00014232); // address of value to load into X
    cpu.writeTwoBytes(0x00014232, 0xFFEF); // value to load into X

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.X, 0xFFEF) << "X register is not set to the correct value";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flags should be set";
    TestUnchangedState(cpu, A, Y, SP);
}


// LDX ABSOLUTE Y-INDEXED TESTS
TEST_F(LDXTest, LoadX_Absolute_YIndexed_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_LDX_ABS_Y, 0x00001023);
    cpu.writeWord(0x00001024, 0x00014232); // partial address of value to load into X
    cpu.Y = 0x0013;
    cpu.writeTwoBytes(0x00014245, 0x1234); // value to load into X

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.X, 0x1234) << "X register is not set to the correct value";
    EXPECT_EQ(cpu.Y, 0x0013) << "Y register should not be altered";
    TestUnchangedState(cpu, A, SP, P);
}

TEST_F(LDXTest, LoadX_Absolute_YIndexed_PAGECROSS) {
    LoadInstruction(cpu, AlienCPU::INS_LDX_ABS_Y, 0x00001023);
    cpu.writeWord(0x00001024, 0x00011232); // partial address of value to load into X
    cpu.Y = 0xF013;
    cpu.writeTwoBytes(0x00020245, 0x1234); // value to load into X

    TestInstruction(cpu, 8, 0x00001028);
    
    EXPECT_EQ(cpu.X, 0x1234) << "X register is not set to the correct value";
    EXPECT_EQ(cpu.Y, 0xF013) << "Y register should not be altered";
    TestUnchangedState(cpu, A, SP, P);
}

TEST_F(LDXTest, LoadX_Absolute_YIndexed_ZFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LDX_ABS_Y, 0x00001023);
    cpu.writeWord(0x00001024, 0x00014232); // partial address of value to load into X
    cpu.Y = 0x0013;
    cpu.writeTwoBytes(0x00014245, 0x0000); // value to load into X

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.X, 0x0000) << "X register is not set to the correct value";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flags should be set";
    EXPECT_EQ(cpu.Y, 0x0013) << "Y register should not be altered";
    TestUnchangedState(cpu, A, SP);
}

TEST_F(LDXTest, LoadX_Absolute_YIndexed_NFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LDX_ABS_Y, 0x00001023);
    cpu.writeWord(0x00001024, 0x00014232); // partial address of value to load into X
    cpu.Y = 0x0013;
    cpu.writeTwoBytes(0x00014245, 0xFFEF); // value to load into X

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.X, 0xFFEF) << "X register is not set to the correct value";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flags should be set";
    EXPECT_EQ(cpu.Y, 0x0013) << "Y register should not be altered";
    TestUnchangedState(cpu, A, SP);
}


// LDX ZEROPAGE TESTS
TEST_F(LDXTest, LoadX_ZeroPage_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_LDX_ZP, 0x0001023);
    cpu.writeTwoBytes(0x00001024, 0x4232); // zp address to the value to load into X
    cpu.writeTwoBytes(0x00004232, 0x2042); // value to load into X

    TestInstruction(cpu, 5, 0x00001026);

    EXPECT_EQ(cpu.X, 0x2042) << "X register is not set to the correct value";
    TestUnchangedState(cpu, A, Y, SP, P);
}

TEST_F(LDXTest, LoadX_ZeroPage_ZFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LDX_ZP, 0x0001023);
    cpu.writeTwoBytes(0x00001024, 0x4232); // zp address to the value to load into X
    cpu.writeTwoBytes(0x00004232, 0x0000); // value to load into X

    TestInstruction(cpu, 5, 0x00001026);

    EXPECT_EQ(cpu.X, 0x0000) << "X register is not set to the correct value";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flags should be set";
    TestUnchangedState(cpu, A, Y, SP);
}

TEST_F(LDXTest, LoadX_ZeroPage_NFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LDX_ZP, 0x0001023);
    cpu.writeTwoBytes(0x00001024, 0x4232); // zp address to the value to load into X
    cpu.writeTwoBytes(0x00004232, 0xFFEF); // value to load into X

    TestInstruction(cpu, 5, 0x00001026);

    EXPECT_EQ(cpu.X, 0xFFEF) << "X register is not set to the correct value";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flags should be set";
    TestUnchangedState(cpu, A, Y, SP);
}


// LDX ZEROPAGE Y-INDEXED TESTS
TEST_F(LDXTest, LoadX_ZeroPage_YIndexed_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_LDX_ZP_Y, 0x0001023);
    cpu.writeTwoBytes(0x00001024, 0x4232); // partial zp address to the value to load into X
    cpu.Y = 0x0013;
    cpu.writeTwoBytes(0x00004245, 0x2042); // value to load into X

    TestInstruction(cpu, 6, 0x00001026);

    EXPECT_EQ(cpu.X, 0x2042) << "X register is not set to the correct value";
    EXPECT_EQ(cpu.Y, 0x0013) << "Y register should not be altered";
    TestUnchangedState(cpu, A, SP, P);
}

TEST_F(LDXTest, LoadX_ZeroPage_YIndexed_WRAPAROUND) {
    LoadInstruction(cpu, AlienCPU::INS_LDX_ZP_Y, 0x0001023);
    cpu.writeTwoBytes(0x00001024, 0x1232); // partial zp address to the value to load into X
    cpu.Y = 0xF013; 
    cpu.writeTwoBytes(0x00000245, 0x2042); // value to load into X

    TestInstruction(cpu, 6, 0x00001026);

    EXPECT_EQ(cpu.X, 0x2042) << "X register is not set to the correct value";
    EXPECT_EQ(cpu.Y, 0xF013) << "Y register should not be altered";
    TestUnchangedState(cpu, A, SP, P);
}

TEST_F(LDXTest, LoadX_ZeroPage_YIndexed_ZFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LDX_ZP_Y, 0x0001023);
    cpu.writeTwoBytes(0x00001024, 0x4232); // partial zp address to the value to load into X
    cpu.Y = 0x0013;
    cpu.writeTwoBytes(0x00004245, 0x0000); // value to load to X

    TestInstruction(cpu, 6, 0x00001026);

    EXPECT_EQ(cpu.X, 0x0000) << "X register is not set to the correct value";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flags should be set";
    EXPECT_EQ(cpu.Y, 0x0013) << "Y register should not be altered";
    TestUnchangedState(cpu, A, SP);
}

TEST_F(LDXTest, LoadX_ZeroPage_YIndexed_NFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LDX_ZP_Y, 0x0001023);
    cpu.writeTwoBytes(0x00001024, 0x4232); // partial zp address to the value to load into X
    cpu.Y = 0x0013;
    cpu.writeTwoBytes(0x00004245, 0xFFEF); // value to load to X

    TestInstruction(cpu, 6, 0x00001026);

    EXPECT_EQ(cpu.X, 0xFFEF) << "X register is not set to the correct value";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only default and negative flags should be set";
    EXPECT_EQ(cpu.Y, 0x0013) << "Y register should not be altered";
    TestUnchangedState(cpu, A, SP);
}