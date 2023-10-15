#include <AlienCPUTest.h>

class ADCTest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};


// ADC IMMEDIATE TESTS
TEST_F(ADCTest, AddWithCarry_Immediate_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_ADC_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x2034); // value to add to accumulator
    cpu.setFlag(cpu.C_FLAG, true);
    cpu.A = 0x0012;

    TestInstruction(cpu, 3, 0x00001026);

    EXPECT_EQ(cpu.A, 0x2047) << "Accumulator should be incremented by 0x2035";
    TestUnchangedState(cpu, X, Y, SP, P);
}

TEST_F(ADCTest, AddWithCarry_Immediate_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ADC_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x0000); // value to add to accumulator
    cpu.A = 0x0000;

    TestInstruction(cpu, 3, 0x00001026);

    EXPECT_EQ(cpu.A, 0x0000) << "Accumulator should be incremented by 0x0000";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flag should be set";
    TestUnchangedState(cpu, X, Y, SP);
}

TEST_F(ADCTest, AddWithCarry_Immediate_CARRYFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ADC_IMM, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x0001); // value to add to accumulator
    cpu.A = 0xFFFF;

    TestInstruction(cpu, 3, 0x00001026);

    EXPECT_EQ(cpu.A, 0x0000) << "Accumulator should be incremented by 0x0001";
    EXPECT_EQ(cpu.P, 0b00100011) << "Only default, zero, and carry flag should be set";
    TestUnchangedState(cpu, X, Y, SP);
}


// ADC ABSOLUTE TESTS
TEST_F(ADCTest, AddWithCarry_Absolute_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_ADC_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012034); // address of value to add to accumulator
    cpu.writeTwoBytes(0x00012034, 0x1234); // value to add to accumulator
    cpu.setFlag(cpu.C_FLAG, true);
    cpu.A = 0x0012;

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.A, 0x1247) << "Accumulator should be incremented by 0x1235";
    TestUnchangedState(cpu, X, Y, SP, P);
}

TEST_F(ADCTest, AddWithCarry_Absolute_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ADC_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012034); // address of value to add to accumulator
    cpu.writeTwoBytes(0x00012034, 0x0000); // value to add to accumulator
    cpu.A = 0x0000;

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.A, 0x0000) << "Accumulator should be incremented by 0x1235";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and carry flag should be set";
    TestUnchangedState(cpu, X, Y, SP);
}

TEST_F(ADCTest, AddWithCarry_Absolute_CARRYFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ADC_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012034); // address of value to add to accumulator
    cpu.writeTwoBytes(0x00012034, 0x0001); // value to add to accumulator
    cpu.A = 0xFFFF;

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.A, 0x0000) << "Accumulator should be incremented by 0x0001";
    EXPECT_EQ(cpu.P, 0b00100011) << "Only default, zero, and carry flag should be set";
    TestUnchangedState(cpu, X, Y, SP);
}


// ADC ABSOLUTE XINDEXED TESTS
TEST_F(ADCTest, AddWithCarry_AbsoluteXIndexed_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_ADC_ABS_X, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012034); // partial address of value to add to accumulator
    cpu.X = 0x0001;
    cpu.writeTwoBytes(0x00012035, 0x1234); // value to add to accumulator
    cpu.setFlag(cpu.C_FLAG, true);
    cpu.A = 0x0012;

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.A, 0x1247) << "Accumulator should be incremented by 0x1235";
    EXPECT_EQ(cpu.X, 0x0001) << "X register should be unchanged";
    TestUnchangedState(cpu, Y, SP, P);
}

TEST_F(ADCTest, AddWithCarry_AbsoluteXIndexed_PAGECROSSING) {
    LoadInstruction(cpu, AlienCPU::INS_ADC_ABS_X, 0x00001023);
    cpu.writeWord(0x00001024, 0x0001FFFF); // partial address of value to add to accumulator
    cpu.X = 0x0001;
    cpu.writeTwoBytes(0x00020000, 0x1234); // value to add to accumulator
    cpu.setFlag(cpu.C_FLAG, true);
    cpu.A = 0x0012;

    TestInstruction(cpu, 9, 0x00001028);

    EXPECT_EQ(cpu.A, 0x1247) << "Accumulator should be incremented by 0x1235";
    EXPECT_EQ(cpu.X, 0x0001) << "X register should be unchanged";
    TestUnchangedState(cpu, Y, SP, P);
}

TEST_F(ADCTest, AddWithCarry_AbsoluteXIndexed_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ADC_ABS_X, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012034); // partial address of value to add to accumulator
    cpu.X = 0x0001;
    cpu.writeTwoBytes(0x00012035, 0x0000); // value to add to accumulator
    cpu.A = 0x0000;

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.A, 0x0000) << "Accumulator should be incremented by 0x0000";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and carry flag should be set";
    EXPECT_EQ(cpu.X, 0x0001) << "X register should be unchanged";
    TestUnchangedState(cpu, Y, SP);
}

TEST_F(ADCTest, AddWithCarry_AbsoluteXIndexed_CARRYFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ADC_ABS_X, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012034); // partial address of value to add to accumulator
    cpu.X = 0x0001;
    cpu.writeTwoBytes(0x00012035, 0x0001); // value to add to accumulator
    cpu.A = 0xFFFF;

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.A, 0x0000) << "Accumulator should be incremented by 0x0001";
    EXPECT_EQ(cpu.P, 0b00100011) << "Only default, zero, and carry flag should be set";
    EXPECT_EQ(cpu.X, 0x0001) << "X register should be unchanged";
    TestUnchangedState(cpu, Y, SP);
}


// ADC ABSOLUTE YINDEXED TESTS
TEST_F(ADCTest, AddWithCarry_AbsoluteYIndexed_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_ADC_ABS_Y, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012034); // partial address of value to add to accumulator
    cpu.Y = 0x0001;
    cpu.writeTwoBytes(0x00012035, 0x1234); // value to add to accumulator
    cpu.A = 0x0012;
    cpu.setFlag(cpu.C_FLAG, true);

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.A, 0x1247) << "Accumulator should be incremented by 0x1235";
    EXPECT_EQ(cpu.Y, 0x0001) << "Y register should be unchanged";
    TestUnchangedState(cpu, X, SP, P);
}

TEST_F(ADCTest, AddWithCarry_AbsoluteYIndexed_PAGECROSSING) {
    LoadInstruction(cpu, AlienCPU::INS_ADC_ABS_Y, 0x00001023);
    cpu.writeWord(0x00001024, 0x0001FFFF); // partial address of value to add to accumulator
    cpu.Y = 0x0001;
    cpu.writeTwoBytes(0x00020000, 0x1234); // value to add to accumulator
    cpu.A = 0x0012;
    cpu.setFlag(cpu.C_FLAG, true);

    TestInstruction(cpu, 9, 0x00001028);

    EXPECT_EQ(cpu.A, 0x1247) << "Accumulator should be incremented by 0x1235";
    EXPECT_EQ(cpu.Y, 0x0001) << "Y register should be unchanged";
    TestUnchangedState(cpu, X, SP, P);
}

TEST_F(ADCTest, AddWithCarry_AbsoluteYIndexed_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ADC_ABS_Y, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012034); // partial address of value to add to accumulator
    cpu.Y = 0x0001;
    cpu.writeTwoBytes(0x00012035, 0x0000); // value to add to accumulator
    cpu.A = 0x0000;

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.A, 0x0000) << "Accumulator should be incremented by 0x0000";
    EXPECT_EQ(cpu.Y, 0x0001) << "Y register should be unchanged";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flag should be set";
    TestUnchangedState(cpu, X, SP);
}

TEST_F(ADCTest, AddWithCarry_AbsoluteYIndexed_CARRYFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ADC_ABS_Y, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012034); // partial address of value to add to accumulator
    cpu.Y = 0x0001;
    cpu.writeTwoBytes(0x00012035, 0x0001); // value to add to accumulator
    cpu.A = 0xFFFF;

    TestInstruction(cpu, 7, 0x00001028);

    EXPECT_EQ(cpu.A, 0x0000) << "Accumulator should be incremented by 0x0001";
    EXPECT_EQ(cpu.Y, 0x0001) << "Y register should be unchanged";
    EXPECT_EQ(cpu.P, 0b00100011) << "Only default, zero, and carry flag should be set";
    TestUnchangedState(cpu, X, SP);
}


// ADC XINDEXED INDIRECT TESTS
TEST_F(ADCTest, AddWithCarry_XIndexedIndirect_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_ADC_X_IND, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1234); // partial zp address of address to value to add to accumulator
    cpu.X = 0x0001;
    cpu.writeWord(0x00001235, 0x00012345); // address of value to add to accumulator
    cpu.writeTwoBytes(0x00012345, 0x1234); // value to add to accumulator
    cpu.A = 0x0012;
    cpu.setFlag(cpu.C_FLAG, true);

    TestInstruction(cpu, 10, 0x00001026);

    EXPECT_EQ(cpu.A, 0x1247) << "Accumulator should be incremented by 0x1235";
    EXPECT_EQ(cpu.X, 0x0001) << "X register should be unchanged";
    TestUnchangedState(cpu, Y, SP, P);
}

TEST_F(ADCTest, AddWithCarry_XIndexedIndirect_WRAPAROUND) {
    LoadInstruction(cpu, AlienCPU::INS_ADC_X_IND, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x0002); // partial zp address of address to value to add to accumulator
    cpu.X = 0xFFFF;
    cpu.writeWord(0x00000001, 0x00012345); // address of value to add to accumulator
    cpu.writeTwoBytes(0x00012345, 0x1234); // value to add to accumulator
    cpu.A = 0x0012;
    cpu.setFlag(cpu.C_FLAG, true);

    TestInstruction(cpu, 10, 0x00001026);

    EXPECT_EQ(cpu.A, 0x1247) << "Accumulator should be incremented by 0x1235";
    EXPECT_EQ(cpu.X, 0xFFFF) << "X register should be unchanged";
    TestUnchangedState(cpu, Y, SP, P);
}

TEST_F(ADCTest, AddWithCarry_XIndexedIndirect_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ADC_X_IND, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1234); // partial zp address of address to value to add to accumulator
    cpu.X = 0x0001;
    cpu.writeWord(0x00001235, 0x00012345); // address of value to add to accumulator
    cpu.writeTwoBytes(0x00012345, 0x0000); // value to add to accumulator
    cpu.A = 0x0000;

    TestInstruction(cpu, 10, 0x00001026);

    EXPECT_EQ(cpu.A, 0x0000) << "Accumulator should be incremented by 0x0000";
    EXPECT_EQ(cpu.X, 0x0001) << "X register should be unchanged";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flag should be set";
    TestUnchangedState(cpu, Y, SP);
}

TEST_F(ADCTest, AddWithCarry_XIndexedIndirect_CARRYFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ADC_X_IND, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1234); // partial zp address of address to value to add to accumulator
    cpu.X = 0x0001;
    cpu.writeWord(0x00001235, 0x00012345); // address of value to add to accumulator
    cpu.writeTwoBytes(0x00012345, 0x0001); // value to add to accumulator
    cpu.A = 0xFFFF;
    cpu.setFlag(cpu.C_FLAG, true);

    TestInstruction(cpu, 10, 0x00001026);

    EXPECT_EQ(cpu.A, 0x0001) << "Accumulator should be incremented by 0x0001";
    EXPECT_EQ(cpu.X, 0x0001) << "X register should be unchanged";
    EXPECT_EQ(cpu.P, 0b00100001) << "Only default and carry flag should be set";
    TestUnchangedState(cpu, Y, SP);
}


// ADC INDIRECT YINDEXED TESTS
TEST_F(ADCTest, AddWithCarry_IndirectYIndexed_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_ADC_IND_Y, 0x00011234);
    cpu.writeTwoBytes(0x00011235, 0x1234); // zp address of partial address of value to add to accumulator
    cpu.writeWord(0x00001234, 0x00012345); // partial address of value to add to accumulator
    cpu.Y = 0x0001;
    cpu.writeTwoBytes(0x00012346, 0x1234); // value to add to accumulator
    cpu.A = 0x0012;
    cpu.setFlag(cpu.C_FLAG, true);

    TestInstruction(cpu, 9, 0x00011237);

    EXPECT_EQ(cpu.A, 0x1247) << "Accumulator should be incremented by 0x1235";
    EXPECT_EQ(cpu.Y, 0x0001) << "Y register should be unchanged";
    TestUnchangedState(cpu, X, SP, P);
}

TEST_F(ADCTest, AddWithCarry_IndirectYIndexed_PAGECROSSING) {
    LoadInstruction(cpu, AlienCPU::INS_ADC_IND_Y, 0x00011234);
    cpu.writeTwoBytes(0x00011235, 0x1234); // zp address of partial address of value to add to accumulator
    cpu.writeWord(0x00001234, 0x00010001); // partial address of value to add to accumulator
    cpu.Y = 0xFFFF;
    cpu.writeTwoBytes(0x00020000, 0x1234); // value to add to accumulator
    cpu.A = 0x0012;
    cpu.setFlag(cpu.C_FLAG, true);

    TestInstruction(cpu, 11, 0x00011237);

    EXPECT_EQ(cpu.A, 0x1247) << "Accumulator should be incremented by 0x1235";
    EXPECT_EQ(cpu.Y, 0xFFFF) << "Y register should be unchanged";
    TestUnchangedState(cpu, X, SP, P);
}

TEST_F(ADCTest, AddWithCarry_IndirectYIndexed_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ADC_IND_Y, 0x00011234);
    cpu.writeTwoBytes(0x00011235, 0x1234); // zp address of partial address of value to add to accumulator
    cpu.writeWord(0x00001234, 0x00012345); // partial address of value to add to accumulator
    cpu.Y = 0x0001;
    cpu.writeTwoBytes(0x00012346, 0x0000); // value to add to accumulator
    cpu.A = 0x0000;

    TestInstruction(cpu, 9, 0x00011237);

    EXPECT_EQ(cpu.A, 0x0000) << "Accumulator should be incremented by 0x0000";
    EXPECT_EQ(cpu.Y, 0x0001) << "Y register should be unchanged";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flag should be set";
    TestUnchangedState(cpu, X, SP);
}

TEST_F(ADCTest, AddWithCarry_IndirectYIndexed_CARRYFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ADC_IND_Y, 0x00011234);
    cpu.writeTwoBytes(0x00011235, 0x1234); // zp address of partial address of value to add to accumulator
    cpu.writeWord(0x00001234, 0x00012345); // partial address of value to add to accumulator
    cpu.Y = 0x0001;
    cpu.writeTwoBytes(0x00012346, 0x0001); // value to add to accumulator
    cpu.A = 0xFFFF;
    cpu.setFlag(cpu.C_FLAG, true);

    TestInstruction(cpu, 9, 0x00011237);

    EXPECT_EQ(cpu.A, 0x0001) << "Accumulator should be incremented by 0x0001";
    EXPECT_EQ(cpu.Y, 0x0001) << "Y register should be unchanged";
    EXPECT_EQ(cpu.P, 0b00100001) << "Only default and carry flag should be set";
    TestUnchangedState(cpu, X, SP);
}


// ADC ZEROPAGE TESTS
TEST_F(ADCTest, AddWithCarry_ZeroPage_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_ADC_ZP, 0x00011234);
    cpu.writeTwoBytes(0x00011235, 0x1234); // zp address of value to add to accumulator
    cpu.writeTwoBytes(0x00001234, 0x1234); // value to add to accumulator
    cpu.A = 0x0012;
    cpu.setFlag(cpu.C_FLAG, true);

    TestInstruction(cpu, 5, 0x00011237);

    EXPECT_EQ(cpu.A, 0x1247) << "Accumulator should be incremented by 0x1235";
    TestUnchangedState(cpu, X, Y, SP, P);
}

TEST_F(ADCTest, AddWithCarry_ZeroPage_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ADC_ZP, 0x00011234);
    cpu.writeTwoBytes(0x00011235, 0x1234); // zp address of value to add to accumulator
    cpu.writeTwoBytes(0x00001234, 0x0000); // value to add to accumulator
    cpu.A = 0x0000;

    TestInstruction(cpu, 5, 0x00011237);

    EXPECT_EQ(cpu.A, 0x0000) << "Accumulator should be incremented by 0x0000";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flag should be set";
    TestUnchangedState(cpu, X, Y, SP);
}

TEST_F(ADCTest, AddWithCarry_ZeroPage_CARRYFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ADC_ZP, 0x00011234);
    cpu.writeTwoBytes(0x00011235, 0x1234); // zp address of value to add to accumulator
    cpu.writeTwoBytes(0x00001234, 0x0001); // value to add to accumulator
    cpu.A = 0xFFFF;
    cpu.setFlag(cpu.C_FLAG, true);

    TestInstruction(cpu, 5, 0x00011237);

    EXPECT_EQ(cpu.A, 0x0001) << "Accumulator should be incremented by 0x0001";
    EXPECT_EQ(cpu.P, 0b00100001) << "Only default and carry flag should be set";
    TestUnchangedState(cpu, X, Y, SP);
}


// ADC ZEROPAGE XINDEXED TESTS
TEST_F(ADCTest, AddWithCarry_ZeroPageXIndexed_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_ADC_ZP_X, 0x00011234);
    cpu.writeTwoBytes(0x00011235, 0x1234); // partial zp address to the value to add to accumulator
    cpu.X = 0x0001;
    cpu.writeTwoBytes(0x00001235, 0x1234); // value to add to accumulator
    cpu.A = 0x0012;
    cpu.setFlag(cpu.C_FLAG, true);

    TestInstruction(cpu, 6, 0x00011237);

    EXPECT_EQ(cpu.A, 0x1247) << "Accumulator should be incremented by 0x1235";
    EXPECT_EQ(cpu.X, 0x0001) << "X register should be unchanged";
    TestUnchangedState(cpu, Y, SP, P);
}

TEST_F(ADCTest, AddWithCarry_ZeroPageXIndexed_WRAPAROUND) {
    LoadInstruction(cpu, AlienCPU::INS_ADC_ZP_X, 0x00011234);
    cpu.writeTwoBytes(0x00011235, 0x0001); // partial zp address to the value to add to accumulator
    cpu.X = 0xFFFF;
    cpu.writeTwoBytes(0x00000000, 0x1234); // value to add to accumulator
    cpu.A = 0x0012;
    cpu.setFlag(cpu.C_FLAG, true);

    TestInstruction(cpu, 6, 0x00011237);

    EXPECT_EQ(cpu.A, 0x1247) << "Accumulator should be incremented by 0x1235";
    EXPECT_EQ(cpu.X, 0xFFFF) << "X register should be unchanged";
    TestUnchangedState(cpu, Y, SP, P);
}

TEST_F(ADCTest, AddWithCarry_ZeroPageXIndexed_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ADC_ZP_X, 0x00011234);
    cpu.writeTwoBytes(0x00011235, 0x1234); // partial zp address to the value to add to accumulator
    cpu.X = 0x0001;
    cpu.writeTwoBytes(0x00001235, 0x0000); // value to add to accumulator
    cpu.A = 0x0000;

    TestInstruction(cpu, 6, 0x00011237);

    EXPECT_EQ(cpu.A, 0x0000) << "Accumulator should be incremented by 0x0000";
    EXPECT_EQ(cpu.X, 0x0001) << "X register should be unchanged";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only default and zero flag should be set";
    TestUnchangedState(cpu, Y, SP);
}

TEST_F(ADCTest, AddWithCarry_ZeroPageXIndexed_CARRYFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ADC_ZP_X, 0x00011234);
    cpu.writeTwoBytes(0x00011235, 0x1234); // partial zp address to the value to add to accumulator
    cpu.X = 0x0001;
    cpu.writeTwoBytes(0x00001235, 0x0001); // value to add to accumulator
    cpu.A = 0xFFFF;
    cpu.setFlag(cpu.C_FLAG, true);

    TestInstruction(cpu, 6, 0x00011237);

    EXPECT_EQ(cpu.A, 0x0001) << "Accumulator should be incremented by 0x0001";
    EXPECT_EQ(cpu.X, 0x0001) << "X register should be unchanged";
    EXPECT_EQ(cpu.P, 0b00100001) << "Only default and carry flag should be set";
    TestUnchangedState(cpu, Y, SP);
}