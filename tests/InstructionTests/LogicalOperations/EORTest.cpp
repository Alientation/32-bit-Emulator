#include <gtest/gtest.h>

#include <AlienCPUTest.h>

class EORTest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};


TEST_F(EORTest, ExclusiveOrImmediate_DEFAULT) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_EOR_IMM);
    cpu.writeTwoBytes(0x00001024, 0x1234);
    cpu.A = 0x5678;

    cpu.start(3);

    EXPECT_EQ(cpu.A, 0x1234 ^ 0x5678); // check accumulator is correctly bitwise exclusive or
    EXPECT_EQ(cpu.PC, 0x00001026); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b00100000); // check only default flag set
    EXPECT_EQ(cpu.cycles, 3);
}

TEST_F(EORTest, ExclusiveOrImmediate_ZEROFLAG) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_EOR_IMM);
    cpu.writeTwoBytes(0x00001024, 0x5678);
    cpu.A = 0x5678;

    cpu.start(3);

    EXPECT_EQ(cpu.A, 0x5678 ^ 0x5678); // check accumulator is correctly bitwise exclusive or
    EXPECT_EQ(cpu.PC, 0x00001026); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b00100010); // check only default and zero flag set
    EXPECT_EQ(cpu.cycles, 3);
}

TEST_F(EORTest, ExclusiveOrImmediate_NEGATIVEFLAG) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_EOR_IMM);
    cpu.writeTwoBytes(0x00001024, 0xFF34);
    cpu.A = 0x0078;

    cpu.start(3);

    EXPECT_EQ(cpu.A, 0xFF34 ^ 0x0078); // check accumulator is correctly bitwise exclusive or
    EXPECT_EQ(cpu.PC, 0x00001026); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b10100000); // check only default and negative flag set
    EXPECT_EQ(cpu.cycles, 3);
}


TEST_F(EORTest, ExclusiveOrAbsolute_NORMAL) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_EOR_ABS);
    cpu.writeWord(0x00001024, 0x00012345);
    cpu.writeTwoBytes(0x00012345, 0x1234);
    cpu.A = 0x5678;

    cpu.start(7);

    EXPECT_EQ(cpu.A, 0x1234 ^ 0x5678); // check accumulator is correctly bitwise exclusive or
    EXPECT_EQ(cpu.PC, 0x00001028); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b00100000); // check only default flag set
    EXPECT_EQ(cpu.cycles, 7);
}

TEST_F(EORTest, ExclusiveOrAbsolute_ZEROFLAG) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_EOR_ABS);
    cpu.writeWord(0x00001024, 0x00012345);
    cpu.writeTwoBytes(0x00012345, 0x5678);
    cpu.A = 0x5678;

    cpu.start(7);

    EXPECT_EQ(cpu.A, 0x5678 ^ 0x5678); // check accumulator is correctly bitwise exclusive or
    EXPECT_EQ(cpu.PC, 0x00001028); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b00100010); // check only default and zero flag set
    EXPECT_EQ(cpu.cycles, 7);
}

TEST_F(EORTest, ExclusiveOrAbsolute_NEGATIVEFLAG) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_EOR_ABS);
    cpu.writeWord(0x00001024, 0x00012345);
    cpu.writeTwoBytes(0x00012345, 0xFF34);
    cpu.A = 0x0078;

    cpu.start(7);

    EXPECT_EQ(cpu.A, 0xFF34 ^ 0x0078); // check accumulator is correctly bitwise exclusive or
    EXPECT_EQ(cpu.PC, 0x00001028); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b10100000); // check only default and negative flag set
    EXPECT_EQ(cpu.cycles, 7);
}


TEST_F(EORTest, ExclusiveOrAbsoluteXIndexed_NORMAL) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_EOR_ABS_X);
    cpu.writeWord(0x00001024, 0x00012345);
    cpu.X = 0x0002;
    cpu.writeTwoBytes(0x00012347, 0x1234);
    cpu.A = 0x5678;

    cpu.start(7);

    EXPECT_EQ(cpu.A, 0x1234 ^ 0x5678); // check accumulator is correctly bitwise exclusive or
    EXPECT_EQ(cpu.X, 0x0002); // check X is unchanged
    EXPECT_EQ(cpu.PC, 0x00001028); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b00100000); // check only default flag set
    EXPECT_EQ(cpu.cycles, 7);
}

TEST_F(EORTest, ExclusiveOrAbsoluteXIndexed_PAGECROSSING) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_EOR_ABS_X);
    cpu.writeWord(0x00001024, 0x00000001);
    cpu.X = 0xFFFF;
    cpu.writeTwoBytes(0x00010000, 0x1234);
    cpu.A = 0x5678;

    cpu.start(9);

    EXPECT_EQ(cpu.A, 0x1234 ^ 0x5678); // check accumulator is correctly bitwise exclusive or
    EXPECT_EQ(cpu.X, 0xFFFF); // check X is unchanged
    EXPECT_EQ(cpu.PC, 0x00001028); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b00100000); // check only default flag set
    EXPECT_EQ(cpu.cycles, 9);
}

TEST_F(EORTest, ExclusiveOrAbsoluteXIndexed_ZEROFLAG) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_EOR_ABS_X);
    cpu.writeWord(0x00001024, 0x00012345);
    cpu.X = 0x0002;
    cpu.writeTwoBytes(0x00012347, 0x5678);
    cpu.A = 0x5678;

    cpu.start(7);

    EXPECT_EQ(cpu.A, 0x5678 ^ 0x5678); // check accumulator is correctly bitwise exclusive or
    EXPECT_EQ(cpu.X, 0x0002); // check X is unchanged
    EXPECT_EQ(cpu.PC, 0x00001028); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b00100010); // check only default and zero flag set
    EXPECT_EQ(cpu.cycles, 7);
}

TEST_F(EORTest, ExclusiveOrAbsoluteXIndexed_NEGATIVEFLAG) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_EOR_ABS_X);
    cpu.writeWord(0x00001024, 0x00012345);
    cpu.X = 0x0002;
    cpu.writeTwoBytes(0x00012347, 0xFF34);
    cpu.A = 0x0078;

    cpu.start(7);

    EXPECT_EQ(cpu.A, 0xFF34 ^ 0x0078); // check accumulator is correctly bitwise exclusive or
    EXPECT_EQ(cpu.X, 0x0002); // check X is unchanged
    EXPECT_EQ(cpu.PC, 0x00001028); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b10100000); // check only default and negative flag set
    EXPECT_EQ(cpu.cycles, 7);
}


TEST_F(EORTest, ExclusiveOrAbsoluteYIndexed_NORMAL) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_EOR_ABS_Y);
    cpu.writeWord(0x00001024, 0x00012345);
    cpu.Y = 0x0002;
    cpu.writeTwoBytes(0x00012347, 0x1234);
    cpu.A = 0x5678;

    cpu.start(7);

    EXPECT_EQ(cpu.A, 0x1234 ^ 0x5678); // check accumulator is correctly bitwise exclusive or
    EXPECT_EQ(cpu.Y, 0x0002); // check Y is unchanged
    EXPECT_EQ(cpu.PC, 0x00001028); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b00100000); // check only default flag set
    EXPECT_EQ(cpu.cycles, 7);
}

TEST_F(EORTest, ExclusiveOrAbsoluteYIndexed_PAGECROSSING) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_EOR_ABS_Y);
    cpu.writeWord(0x00001024, 0x00000001);
    cpu.Y = 0xFFFF;
    cpu.writeTwoBytes(0x00010000, 0x1234);
    cpu.A = 0x5678;

    cpu.start(9);

    EXPECT_EQ(cpu.A, 0x1234 ^ 0x5678); // check accumulator is correctly bitwise exclusive or
    EXPECT_EQ(cpu.Y, 0xFFFF); // check Y is unchanged
    EXPECT_EQ(cpu.PC, 0x00001028); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b00100000); // check only default flag set
    EXPECT_EQ(cpu.cycles, 9);
}

TEST_F(EORTest, ExclusiveOrAbsoluteYIndexed_ZEROFLAG) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_EOR_ABS_Y);
    cpu.writeWord(0x00001024, 0x00012345);
    cpu.Y = 0x0002;
    cpu.writeTwoBytes(0x00012347, 0x5678);
    cpu.A = 0x5678;

    cpu.start(7);

    EXPECT_EQ(cpu.A, 0x5678 ^ 0x5678); // check accumulator is correctly bitwise exclusive or
    EXPECT_EQ(cpu.Y, 0x0002); // check Y is unchanged
    EXPECT_EQ(cpu.PC, 0x00001028); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b00100010); // check only default and zero flag set
    EXPECT_EQ(cpu.cycles, 7);
}

TEST_F(EORTest, ExclusiveOrAbsoluteYIndexed_NEGATIVEFLAG) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_EOR_ABS_Y);
    cpu.writeWord(0x00001024, 0x00012345);
    cpu.Y = 0x0002;
    cpu.writeTwoBytes(0x00012347, 0xFF34);
    cpu.A = 0x0078;

    cpu.start(7);

    EXPECT_EQ(cpu.A, 0xFF34 ^ 0x0078); // check accumulator is correctly bitwise exclusive or
    EXPECT_EQ(cpu.Y, 0x0002); // check Y is unchanged
    EXPECT_EQ(cpu.PC, 0x00001028); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b10100000); // check only default and negative flag set
    EXPECT_EQ(cpu.cycles, 7);
}


TEST_F(EORTest, ExclusiveOrXIndexedIndirect_NORMAL) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_EOR_X_IND);
    cpu.writeTwoBytes(0x00001024, 0x1234);
    cpu.X = 0x0002;
    cpu.writeWord(0x00001236, 0x00012345);
    cpu.writeTwoBytes(0x00012345, 0x1234);
    cpu.A = 0x5678;

    cpu.start(10);

    EXPECT_EQ(cpu.A, 0x1234 ^ 0x5678); // check accumulator is correctly bitwise exclusive or
    EXPECT_EQ(cpu.X, 0x0002); // check X is unchanged
    EXPECT_EQ(cpu.PC, 0x00001026); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b00100000); // check only default flag set
    EXPECT_EQ(cpu.cycles, 10);
}

TEST_F(EORTest, ExclusiveOrXIndexedIndirect_ZEROFLAG) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_EOR_X_IND);
    cpu.writeTwoBytes(0x00001024, 0x1234);
    cpu.X = 0x0002;
    cpu.writeWord(0x00001236, 0x00012345);
    cpu.writeTwoBytes(0x00012345, 0x5678);
    cpu.A = 0x5678;

    cpu.start(10);

    EXPECT_EQ(cpu.A, 0x5678 ^ 0x5678); // check accumulator is correctly bitwise exclusive or
    EXPECT_EQ(cpu.X, 0x0002); // check X is unchanged
    EXPECT_EQ(cpu.PC, 0x00001026); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b00100010); // check only default and zero flag set
    EXPECT_EQ(cpu.cycles, 10);
}

TEST_F(EORTest, ExclusiveOrXIndexedIndirect_NEGATIVEFLAG) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_EOR_X_IND);
    cpu.writeTwoBytes(0x00001024, 0x1234);
    cpu.X = 0x0002;
    cpu.writeWord(0x00001236, 0x00012345);
    cpu.writeTwoBytes(0x00012345, 0xFF34);
    cpu.A = 0x0078;

    cpu.start(10);

    EXPECT_EQ(cpu.A, 0xFF34 ^ 0x0078); // check accumulator is correctly bitwise exclusive or
    EXPECT_EQ(cpu.X, 0x0002); // check X is unchanged
    EXPECT_EQ(cpu.PC, 0x00001026); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b10100000); // check only default and negative flag set
    EXPECT_EQ(cpu.cycles, 10);
}


TEST_F(EORTest, ExclusiveOrIndirectYIndexed_NORMAL) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_EOR_IND_Y);
    cpu.writeTwoBytes(0x00001024, 0x1234);
    cpu.writeWord(0x00001234, 0x00012345);
    cpu.Y = 0x0002;
    cpu.writeTwoBytes(0x00012347, 0x1234);
    cpu.A = 0x5678;

    cpu.start(9);

    EXPECT_EQ(cpu.A, 0x1234 ^ 0x5678); // check accumulator is correctly bitwise exclusive or
    EXPECT_EQ(cpu.Y, 0x0002); // check Y is unchanged
    EXPECT_EQ(cpu.PC, 0x00001026); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b00100000); // check only default flag set
    EXPECT_EQ(cpu.cycles, 9);
}

TEST_F(EORTest, ExclusiveOrIndirectYIndexed_PAGECROSSING) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_EOR_IND_Y);
    cpu.writeTwoBytes(0x00001024, 0x1234);
    cpu.writeWord(0x00001234, 0x00000001);
    cpu.Y = 0xFFFF;
    cpu.writeTwoBytes(0x00010000, 0x1234);
    cpu.A = 0x5678;

    cpu.start(11);

    EXPECT_EQ(cpu.A, 0x1234 ^ 0x5678); // check accumulator is correctly bitwise exclusive or
    EXPECT_EQ(cpu.Y, 0xFFFF); // check Y is unchanged
    EXPECT_EQ(cpu.PC, 0x00001026); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b00100000); // check only default flag set
    EXPECT_EQ(cpu.cycles, 11);
}

TEST_F(EORTest, ExclusiveOrIndirectYIndexed_ZEROFLAG) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_EOR_IND_Y);
    cpu.writeTwoBytes(0x00001024, 0x1234);
    cpu.writeWord(0x00001234, 0x00012345);
    cpu.Y = 0x0002;
    cpu.writeTwoBytes(0x00012347, 0x5678);
    cpu.A = 0x5678;

    cpu.start(9);

    EXPECT_EQ(cpu.A, 0x5678 ^ 0x5678); // check accumulator is correctly bitwise exclusive or
    EXPECT_EQ(cpu.Y, 0x0002); // check Y is unchanged
    EXPECT_EQ(cpu.PC, 0x00001026); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b00100010); // check only default and zero flag set
    EXPECT_EQ(cpu.cycles, 9);
}

TEST_F(EORTest, ExclusiveOrIndirectYIndexed_NEGATIVEFLAG) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_EOR_IND_Y);
    cpu.writeTwoBytes(0x00001024, 0x1234);
    cpu.writeWord(0x00001234, 0x00012345);
    cpu.Y = 0x0002;
    cpu.writeTwoBytes(0x00012347, 0xFF34);
    cpu.A = 0x0078;

    cpu.start(9);

    EXPECT_EQ(cpu.A, 0xFF34 ^ 0x0078); // check accumulator is correctly bitwise exclusive or
    EXPECT_EQ(cpu.Y, 0x0002); // check Y is unchanged
    EXPECT_EQ(cpu.PC, 0x00001026); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b10100000); // check only default and negative flag set
    EXPECT_EQ(cpu.cycles, 9);
}


TEST_F(EORTest, ExclusiveOrZeroPage_NORMAL) {
    // setting reset vector to begin processing instructions at 0x00011023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00011023);
    cpu.writeByte(0x00011023, AlienCPU::INS_EOR_ZP);
    cpu.writeTwoBytes(0x00011024, 0x1234);
    cpu.writeTwoBytes(0x00001234, 0x1234);
    cpu.A = 0x5678;

    cpu.start(5);

    EXPECT_EQ(cpu.A, 0x1234 ^ 0x5678); // check accumulator is correctly bitwise exclusive or
    EXPECT_EQ(cpu.PC, 0x00011026); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b00100000); // check only default flag set
    EXPECT_EQ(cpu.cycles, 5);
}

TEST_F(EORTest, ExclusiveOrZeroPage_ZEROFLAG) {
    // setting reset vector to begin processing instructions at 0x00011023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00011023);
    cpu.writeByte(0x00011023, AlienCPU::INS_EOR_ZP);
    cpu.writeTwoBytes(0x00011024, 0x1234);
    cpu.writeTwoBytes(0x00001234, 0x5678);
    cpu.A = 0x5678;

    cpu.start(5);

    EXPECT_EQ(cpu.A, 0x5678 ^ 0x5678); // check accumulator is correctly bitwise exclusive or
    EXPECT_EQ(cpu.PC, 0x00011026); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b00100010); // check only default and zero flag set
    EXPECT_EQ(cpu.cycles, 5);
}

TEST_F(EORTest, ExclusiveOrZeroPage_NEGATIVEFLAG) {
    // setting reset vector to begin processing instructions at 0x00011023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00011023);
    cpu.writeByte(0x00011023, AlienCPU::INS_EOR_ZP);
    cpu.writeTwoBytes(0x00011024, 0x1234);
    cpu.writeTwoBytes(0x00001234, 0xFF34);
    cpu.A = 0x0078;

    cpu.start(5);

    EXPECT_EQ(cpu.A, 0xFF34 ^ 0x0078); // check accumulator is correctly bitwise exclusive or
    EXPECT_EQ(cpu.PC, 0x00011026); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b10100000); // check only default and negative flag set
    EXPECT_EQ(cpu.cycles, 5);
}


TEST_F(EORTest, ExclusiveOrZeroPageXIndexed_NORMAL) {
    // setting reset vector to begin processing instructions at 0x00011023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00011023);
    cpu.writeByte(0x00011023, AlienCPU::INS_EOR_ZP_X);
    cpu.writeTwoBytes(0x00011024, 0x1234);
    cpu.X = 0x0002;
    cpu.writeTwoBytes(0x00001236, 0x1234);
    cpu.A = 0x5678;

    cpu.start(6);

    EXPECT_EQ(cpu.A, 0x1234 ^ 0x5678); // check accumulator is correctly bitwise exclusive or
    EXPECT_EQ(cpu.X, 0x0002); // check X is unchanged
    EXPECT_EQ(cpu.PC, 0x00011026); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b00100000); // check only default flag set
    EXPECT_EQ(cpu.cycles, 6);
}

TEST_F(EORTest, ExclusiveOrZeroPageXIndexed_PAGEWRAPPING) {
    // setting reset vector to begin processing instructions at 0x00011023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00011023);
    cpu.writeByte(0x00011023, AlienCPU::INS_EOR_ZP_X);
    cpu.writeTwoBytes(0x00011024, 0x0002);
    cpu.X = 0xFFFF;
    cpu.writeTwoBytes(0x00000001, 0x1234);
    cpu.A = 0x5678;

    cpu.start(6);

    EXPECT_EQ(cpu.A, 0x1234 ^ 0x5678); // check accumulator is correctly bitwise exclusive or
    EXPECT_EQ(cpu.X, 0xFFFF); // check X is unchanged
    EXPECT_EQ(cpu.PC, 0x00011026); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b00100000); // check only default flag set
    EXPECT_EQ(cpu.cycles, 6);
}

TEST_F(EORTest, ExclusiveOrZeroPageXIndexed_ZEROFLAG) {
    // setting reset vector to begin processing instructions at 0x00011023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00011023);
    cpu.writeByte(0x00011023, AlienCPU::INS_EOR_ZP_X);
    cpu.writeTwoBytes(0x00011024, 0x1234);
    cpu.X = 0x0002;
    cpu.writeTwoBytes(0x00001236, 0x5678);
    cpu.A = 0x5678;

    cpu.start(6);

    EXPECT_EQ(cpu.A, 0x5678 ^ 0x5678); // check accumulator is correctly bitwise exclusive or
    EXPECT_EQ(cpu.X, 0x0002); // check X is unchanged
    EXPECT_EQ(cpu.PC, 0x00011026); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b00100010); // check only default and zero flag set
    EXPECT_EQ(cpu.cycles, 6);
}

TEST_F(EORTest, ExclusiveOrZeroPageXIndexed_NEGATIVEFLAG) {
    // setting reset vector to begin processing instructions at 0x00011023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00011023);
    cpu.writeByte(0x00011023, AlienCPU::INS_EOR_ZP_X);
    cpu.writeTwoBytes(0x00011024, 0x1234);
    cpu.X = 0x0002;
    cpu.writeTwoBytes(0x00001236, 0xFF34);
    cpu.A = 0x0078;

    cpu.start(6);

    EXPECT_EQ(cpu.A, 0xFF34 ^ 0x0078); // check accumulator is correctly bitwise exclusive or
    EXPECT_EQ(cpu.X, 0x0002); // check X is unchanged
    EXPECT_EQ(cpu.PC, 0x00011026); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b10100000); // check only default and negative flag set
    EXPECT_EQ(cpu.cycles, 6);
}
