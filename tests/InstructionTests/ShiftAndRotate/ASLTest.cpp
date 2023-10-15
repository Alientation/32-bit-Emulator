#include <AlienCPUTest.h>

class ASLTest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};


// ASL ACCUMULATOR TESTS
TEST_F(ASLTest, ArithmeticShiftLeft_Accumulator_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_ASL_ACC, 0x00001023);
    cpu.A = 0b0011001110011010; // value to shift left

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.A, 0b0110011100110100) << "Accumulator should be shifted left";
    EXPECT_EQ(cpu.P, 0b00100000) << "Only the default flag should be set";
}

TEST_F(ASLTest, ArithmeticShiftLeft_Accumulator_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ASL_ACC, 0x00001023);
    cpu.A = 0b0000000000000000; // value to shift left

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.A, 0b0000000000000000) << "Accumulator should be shifted left";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only the default and zero flags should be set";
}

TEST_F(ASLTest, ArithmeticShiftLeft_Accumulator_CARRYFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ASL_ACC, 0x00001023);
    cpu.A = 0b1011001110011010; // value to shift left

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.A, 0b0110011100110100) << "Accumulator should be shifted left";
    EXPECT_EQ(cpu.P, 0b00100001) << "Only the default and carry flags should be set";
}

TEST_F(ASLTest, ArithmeticShiftLeft_Accumulator_NEGATIVEFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ASL_ACC, 0x00001023);
    cpu.A = 0b0111001110011010; // value to shift left

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.A, 0b1110011100110100) << "Accumulator should be shifted left";
    EXPECT_EQ(cpu.P, 0b10100000) << "Only the default and negative flags should be set";
}


// ASL ABSOLUTE TESTS
TEST_F(ASLTest, ArithmeticShiftLeft_Absolute_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_ASL_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012345); // address of value to shift left
    cpu.writeByte(0x00012345, 0b00110111); // value to shift left

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.motherboard.ram[0x00012345], 0b01101110) << "Memory value should be shifted left";
    EXPECT_EQ(cpu.P, 0b00100000) << "Only the default flag should be set";
}

TEST_F(ASLTest, ArithmeticShiftLeft_Absolute_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ASL_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012345); // address of value to shift left
    cpu.writeByte(0x00012345, 0b00000000); // value to shift left

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.motherboard.ram[0x00012345], 0b00000000) << "Memory value should be shifted left";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only the default and zero flags should be set";
}

TEST_F(ASLTest, ArithmeticShiftLeft_Absolute_CARRYFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ASL_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012345); // address of value to shift left
    cpu.writeByte(0x00012345, 0b10110111); // value to shift left

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.motherboard.ram[0x00012345], 0b01101110) << "Memory value should be shifted left";
    EXPECT_EQ(cpu.P, 0b00100001) << "Only the default and carry flags should be set";
}


// ASL ABSOLUTE XINDEXED TESTS
TEST_F(ASLTest, ArithmeticShiftLeft_AbsoluteXIndexed_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_ASL_ABS_X, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012345); // partial address of value to shift left
    cpu.X = 0x02;
    cpu.writeByte(0x00012347, 0b00110111); // value to shift left

    TestInstruction(cpu, 9, 0x00001028);

    EXPECT_EQ(cpu.motherboard.ram[0x00012347], 0b01101110) << "Memory value should be shifted left";
    EXPECT_EQ(cpu.P, 0b00100000) << "Only the default flag should be set";
}

TEST_F(ASLTest, ArithmeticShiftLeft_AbsoluteXIndexed_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ASL_ABS_X, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012345); // partial address of value to shift left
    cpu.X = 0x02;
    cpu.writeByte(0x00012347, 0b00000000); // value to shift left

    TestInstruction(cpu, 9, 0x00001028);

    EXPECT_EQ(cpu.motherboard.ram[0x00012347], 0b00000000) << "Memory value should be shifted left";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only the default and zero flags should be set";
}

TEST_F(ASLTest, ArithmeticShiftLeft_AbsoluteXIndexed_CARRYFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ASL_ABS_X, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012345); // partial address of value to shift left
    cpu.X = 0x02;
    cpu.writeByte(0x00012347, 0b10110111); // value to shift left

    TestInstruction(cpu, 9, 0x00001028);

    EXPECT_EQ(cpu.motherboard.ram[0x00012347], 0b01101110) << "Memory value should be shifted left";
    EXPECT_EQ(cpu.P, 0b00100001) << "Only the default and carry flags should be set";
}


// ASL ZEROPAGE TESTS
TEST_F(ASLTest, ArithmeticShiftLeft_ZeroPage_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_ASL_ZP, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1234); // zp address of value to shift left
    cpu.writeByte(0x00001234, 0b00110111); // value to shift left

    TestInstruction(cpu, 6, 0x00001026);

    EXPECT_EQ(cpu.motherboard.ram[0x00001234], 0b01101110) << "Memory value should be shifted left";
    EXPECT_EQ(cpu.P, 0b00100000) << "Only the default flag should be set";
}

TEST_F(ASLTest, ArithmeticShiftLeft_ZeroPage_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ASL_ZP, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1234); // zp address of value to shift left
    cpu.writeByte(0x00001234, 0b00000000); // value to shift left

    TestInstruction(cpu, 6, 0x00001026);

    EXPECT_EQ(cpu.motherboard.ram[0x00001234], 0b00000000) << "Memory value should be shifted left";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only the default and zero flags should be set";
}

TEST_F(ASLTest, ArithmeticShiftLeft_ZeroPage_CARRYFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ASL_ZP, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1234); // zp address of value to shift left
    cpu.writeByte(0x00001234, 0b10110111); // value to shift left

    TestInstruction(cpu, 6, 0x00001026);

    EXPECT_EQ(cpu.motherboard.ram[0x00001234], 0b01101110) << "Memory value should be shifted left";
    EXPECT_EQ(cpu.P, 0b00100001) << "Only the default and carry flags should be set";
}


// ASL ZEROPAGE XINDEXED TESTS
TEST_F(ASLTest, ArithmeticShiftLeft_ZeroPageXIndexed_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_ASL_ZP_X, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1234); // partial zp address of value to shift left
    cpu.X = 0x02;
    cpu.writeByte(0x00001236, 0b00110111); // value to shift left

    TestInstruction(cpu, 7, 0x00001026);

    EXPECT_EQ(cpu.motherboard.ram[0x00001236], 0b01101110) << "Memory value should be shifted left";
    EXPECT_EQ(cpu.P, 0b00100000) << "Only the default flag should be set";
}

TEST_F(ASLTest, ArithmeticShiftLeft_ZeroPageXIndexed_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ASL_ZP_X, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1234); // partial zp address of value to shift left
    cpu.X = 0x02;
    cpu.writeByte(0x00001236, 0b00000000); // value to shift left

    TestInstruction(cpu, 7, 0x00001026);

    EXPECT_EQ(cpu.motherboard.ram[0x00001236], 0b00000000) << "Memory value should be shifted left";
    EXPECT_EQ(cpu.P, 0b00100010) << "Only the default and zero flags should be set";
}

TEST_F(ASLTest, ArithmeticShiftLeft_ZeroPageXIndexed_CARRYFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ASL_ZP_X, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1234); // partial zp address of value to shift left
    cpu.X = 0x02;
    cpu.writeByte(0x00001236, 0b10110111); // value to shift left

    TestInstruction(cpu, 7, 0x00001026);

    EXPECT_EQ(cpu.motherboard.ram[0x00001236], 0b01101110) << "Memory value should be shifted left";
    EXPECT_EQ(cpu.P, 0b00100001) << "Only the default and carry flags should be set";
}