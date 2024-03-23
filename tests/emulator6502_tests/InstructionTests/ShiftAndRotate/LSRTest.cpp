#include <emulator6502_tests\AlienCPUTest.h>

class LSRTest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};


// LSR ACCUMULATOR TESTS
TEST_F(LSRTest, LogicalShiftRight_Accumulator_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_LSR_ACC, 0x00001023);
    cpu.A = 0b0011001110011010; // value to shift right

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.A, 0b0001100111001101) << "Accumulator should be shifted right";
    TestUnchangedState(cpu, X, Y, SP, P);
}

TEST_F(LSRTest, LogicalShiftRight_Accumulator_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LSR_ACC, 0x00001023);
    cpu.A = 0b0000000000000000; // value to shift right

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.A, 0b0000000000000000) << "Accumulator should be shifted right";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only the default and zero flags should be set";
    TestUnchangedState(cpu, X, Y, SP);
}

TEST_F(LSRTest, LogicalShiftRight_Accumulator_CARRYFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LSR_ACC, 0x00001023);
    cpu.A = 0b0011001110011011; // value to shift right

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.A, 0b0001100111001101) << "Accumulator should be shifted right";
    EXPECT_EQ(cpu.P, 0b00100001) << "Only the default and carry flags should be set";
    TestUnchangedState(cpu, X, Y, SP);
}


// LSR ABSOLUTE TESTS
TEST_F(LSRTest, LogicalShiftRight_Absolute_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_LSR_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012345); // address of the value to shift right
    cpu.writeByte(0x00012345, 0b00110110); // value to shift right

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.motherboard.ram[0x00012345], 0b00011011);
    TestUnchangedState(cpu, A, X, Y, SP, P);
}

TEST_F(LSRTest, LogicalShiftRight_Absolute_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LSR_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012345); // address of the value to shift right
    cpu.writeByte(0x00012345, 0b00000000); // value to shift right

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.motherboard.ram[0x00012345], 0b00000000);
    EXPECT_EQ(cpu.P, 0b00100010) << "Only the default and zero flags should be set";
    TestUnchangedState(cpu, A, X, Y, SP);
}

TEST_F(LSRTest, LogicalShiftRight_Absolute_CARRYFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LSR_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012345); // address of the value to shift right
    cpu.writeByte(0x00012345, 0b00110111); // value to shift right

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.motherboard.ram[0x00012345], 0b00011011);
    EXPECT_EQ(cpu.P, 0b00100001) << "Only the default and carry flags should be set";
    TestUnchangedState(cpu, A, X, Y, SP);
}


// LSR ABSOLUTE XINDEXED TESTS
TEST_F(LSRTest, LogicalShiftRight_AbsoluteXIndexed_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_LSR_ABS_X, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012345); // partial address of the value to shift right
    cpu.X = 0x0002;
    cpu.writeByte(0x00012347, 0b00110110); // value to shift right

    TestInstruction(cpu, 9, 0x00001028);

    EXPECT_EQ(cpu.motherboard.ram[0x00012347], 0b00011011);
    EXPECT_EQ(cpu.X, 0x0002) << "X register should not be altered";
    TestUnchangedState(cpu, A, Y, SP, P);
}

TEST_F(LSRTest, LogicalShiftRight_AbsoluteXIndexed_PAGECROSS) {
    LoadInstruction(cpu, AlienCPU::INS_LSR_ABS_X, 0x00001023);
    cpu.writeWord(0x00001024, 0x00011345); // partial address of the value to shift right
    cpu.X = 0xF002;
    cpu.writeByte(0x00020347, 0b00110110); // value to shift right

    TestInstruction(cpu, 9, 0x00001028);

    EXPECT_EQ(cpu.motherboard.ram[0x00020347], 0b00011011);
    EXPECT_EQ(cpu.X, 0xF002) << "X register should not be altered";
    TestUnchangedState(cpu, A, Y, SP, P);
}

TEST_F(LSRTest, LogicalShiftRight_AbsoluteXIndexed_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LSR_ABS_X, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012345); // partial address of the value to shift right
    cpu.X = 0x0002;
    cpu.writeByte(0x00012347, 0b00000000); // value to shift right

    TestInstruction(cpu, 9, 0x00001028);

    EXPECT_EQ(cpu.motherboard.ram[0x00012347], 0b00000000);
    EXPECT_EQ(cpu.X, 0x0002) << "X register should not be altered";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only the default and zero flags should be set";
    TestUnchangedState(cpu, A, Y, SP);
}

TEST_F(LSRTest, LogicalShiftRight_AbsoluteXIndexed_CARRYFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LSR_ABS_X, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012345); // partial address of the value to shift right
    cpu.X = 0x0002;
    cpu.writeByte(0x00012347, 0b00110111); // value to shift right

    TestInstruction(cpu, 9, 0x00001028);

    EXPECT_EQ(cpu.motherboard.ram[0x00012347], 0b00011011);
    EXPECT_EQ(cpu.X, 0x0002) << "X register should not be altered";
    EXPECT_EQ(cpu.P, 0b00100001) << "Only the default and carry flags should be set";
    TestUnchangedState(cpu, A, Y, SP);
}


// LSR ZEROPAGE TESTS
TEST_F(LSRTest, LogicalShiftRight_ZeroPage_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_LSR_ZP, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1234); // zp address of the value to shift right
    cpu.writeByte(0x00001234, 0b00110110); // value to shift right

    TestInstruction(cpu, 6, 0x00001026);

    EXPECT_EQ(cpu.motherboard.ram[0x00001234], 0b00011011);
    TestUnchangedState(cpu, A, X, Y, SP, P);
}

TEST_F(LSRTest, LogicalShiftRight_ZeroPage_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LSR_ZP, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1234); // zp address of the value to shift right
    cpu.writeByte(0x00001234, 0b00000000); // value to shift right

    TestInstruction(cpu, 6, 0x00001026);

    EXPECT_EQ(cpu.motherboard.ram[0x00001234], 0b00000000);
    EXPECT_EQ(cpu.P, 0b00100010) << "Only the default and zero flags should be set";
    TestUnchangedState(cpu, A, X, Y, SP);
}

TEST_F(LSRTest, LogicalShiftRight_ZeroPage_CARRYFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LSR_ZP, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1234); // zp address of the value to shift right
    cpu.writeByte(0x00001234, 0b00110111); // value to shift right

    TestInstruction(cpu, 6, 0x00001026);

    EXPECT_EQ(cpu.motherboard.ram[0x00001234], 0b00011011);
    EXPECT_EQ(cpu.P, 0b00100001) << "Only the default and carry flags should be set";
    TestUnchangedState(cpu, A, X, Y, SP);
}


// LSR ZEROPAGE XINDEXED TESTS
TEST_F(LSRTest, LogicalShiftRight_ZeroPageXIndexed_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_LSR_ZP_X, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1234); // partial zp address of the value to shift right
    cpu.X = 0x0002;
    cpu.writeByte(0x00001236, 0b00110110); // value to shift right

    TestInstruction(cpu, 7, 0x00001026);

    EXPECT_EQ(cpu.motherboard.ram[0x00001236], 0b00011011);
    EXPECT_EQ(cpu.X, 0x0002) << "X register should not be altered";
    TestUnchangedState(cpu, A, Y, SP, P);
}

TEST_F(LSRTest, LogicalShiftRight_ZeroPageXIndexed_PAGEWRAP) {
    LoadInstruction(cpu, AlienCPU::INS_LSR_ZP_X, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1234); // partial zp address of the value to shift right
    cpu.X = 0xF002;
    cpu.writeByte(0x00000236, 0b00110110); // value to shift right

    TestInstruction(cpu, 7, 0x00001026);

    EXPECT_EQ(cpu.motherboard.ram[0x00000236], 0b00011011);
    EXPECT_EQ(cpu.X, 0xF002) << "X register should not be altered";
    TestUnchangedState(cpu, A, Y, SP, P);
}

TEST_F(LSRTest, LogicalShiftRight_ZeroPageXIndexed_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LSR_ZP_X, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1234); // partial zp address of the value to shift right
    cpu.X = 0x0002;
    cpu.writeByte(0x00001236, 0b00000000); // value to shift right

    TestInstruction(cpu, 7, 0x00001026);

    EXPECT_EQ(cpu.motherboard.ram[0x00001236], 0b00000000);
    EXPECT_EQ(cpu.X, 0x0002) << "X register should not be altered";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only the default and zero flags should be set";
    TestUnchangedState(cpu, A, Y, SP);
}

TEST_F(LSRTest, LogicalShiftRight_ZeroPageXIndexed_CARRYFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_LSR_ZP_X, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1234); // partial zp address of the value to shift right
    cpu.X = 0x0002;
    cpu.writeByte(0x00001236, 0b00110111); // value to shift right

    TestInstruction(cpu, 7, 0x00001026);

    EXPECT_EQ(cpu.motherboard.ram[0x00001236], 0b00011011);
    EXPECT_EQ(cpu.X, 0x0002) << "X register should not be altered";
    EXPECT_EQ(cpu.P, 0b00100001) << "Only the default and carry flags should be set";
    TestUnchangedState(cpu, A, Y, SP);
}