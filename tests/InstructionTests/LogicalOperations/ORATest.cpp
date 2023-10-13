#include <gtest/gtest.h>

#include <AlienCPUTest.h>

class ORATest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};


TEST_F(ORATest, InclusiveOrImmediate_DEFAULT) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_ORA_IMM);
    cpu.writeTwoBytes(0x00001024, 0x1234);
    cpu.A = 0x5678;

    cpu.start(3);

    EXPECT_EQ(cpu.A, 0x1234 | 0x5678); // check accumulator is correctly bitwise inclusive or
    EXPECT_EQ(cpu.PC, 0x00001026); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b00100000); // check only default flag set
    EXPECT_EQ(cpu.cycles, 3);
}

TEST_F(ORATest, InclusiveOrImmediate_ZEROFLAG) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_ORA_IMM);
    cpu.writeTwoBytes(0x00001024, 0x0000);
    cpu.A = 0x0000;

    cpu.start(3);

    EXPECT_EQ(cpu.A, 0x0000 | 0x0000); // check accumulator is correctly bitwise inclusive or
    EXPECT_EQ(cpu.PC, 0x00001026); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b00100010); // check only default and zero flag set
    EXPECT_EQ(cpu.cycles, 3);
}

TEST_F(ORATest, InclusiveOrImmediate_NEGATIVEFLAG) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_ORA_IMM);
    cpu.writeTwoBytes(0x00001024, 0xFF34);
    cpu.A = 0xFF78;

    cpu.start(3);

    EXPECT_EQ(cpu.A, 0xFF34 | 0xFF78); // check accumulator is correctly bitwise inclusive or
    EXPECT_EQ(cpu.PC, 0x00001026); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b10100000); // check only default and negative flag set
    EXPECT_EQ(cpu.cycles, 3);
}


TEST_F(ORATest, InclusiveOrAbsolute_NORMAL) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_ORA_ABS);
    cpu.writeWord(0x00001024, 0x00012345);
    cpu.writeTwoBytes(0x00012345, 0x1234);
    cpu.A = 0x5678;

    cpu.start(7);

    EXPECT_EQ(cpu.A, 0x1234 | 0x5678); // check accumulator is correctly bitwise inclusive or
    EXPECT_EQ(cpu.PC, 0x00001028); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b00100000); // check only default flag set
    EXPECT_EQ(cpu.cycles, 7);
}

TEST_F(ORATest, InclusiveOrAbsolute_ZEROFLAG) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_ORA_ABS);
    cpu.writeWord(0x00001024, 0x00012345);
    cpu.writeTwoBytes(0x00012345, 0x0000);
    cpu.A = 0x0000;

    cpu.start(7);

    EXPECT_EQ(cpu.A, 0x0000 | 0x0000); // check accumulator is correctly bitwise inclusive or
    EXPECT_EQ(cpu.PC, 0x00001028); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b00100010); // check only default and zero flag set
    EXPECT_EQ(cpu.cycles, 7);
}

TEST_F(ORATest, InclusiveOrAbsolute_NEGATIVEFLAG) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_ORA_ABS);
    cpu.writeWord(0x00001024, 0x00012345);
    cpu.writeTwoBytes(0x00012345, 0xFF34);
    cpu.A = 0xFF78;

    cpu.start(7);

    EXPECT_EQ(cpu.A, 0xFF34 | 0xFF78); // check accumulator is correctly bitwise inclusive or
    EXPECT_EQ(cpu.PC, 0x00001028); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b10100000); // check only default and negative flag set
    EXPECT_EQ(cpu.cycles, 7);
}


TEST_F(ORATest, InclusiveOrAbsoluteXIndexed_NORMAL) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_ORA_ABS_X);
    cpu.writeWord(0x00001024, 0x00012345);
    cpu.X = 0x0002;
    cpu.writeTwoBytes(0x00012347, 0x1234);
    cpu.A = 0x5678;

    cpu.start(7);

    EXPECT_EQ(cpu.A, 0x1234 | 0x5678); // check accumulator is correctly bitwise inclusive or
    EXPECT_EQ(cpu.X, 0x0002); // check X is unchanged
    EXPECT_EQ(cpu.PC, 0x00001028); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b00100000); // check only default flag set
    EXPECT_EQ(cpu.cycles, 7);
}

TEST_F(ORATest, InclusiveOrAbsoluteXIndexed_PAGECROSSING) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_ORA_ABS_X);
    cpu.writeWord(0x00001024, 0x00000001);
    cpu.X = 0xFFFF;
    cpu.writeTwoBytes(0x00010000, 0x1234);
    cpu.A = 0x5678;

    cpu.start(9);

    EXPECT_EQ(cpu.A, 0x1234 | 0x5678); // check accumulator is correctly bitwise inclusive or
    EXPECT_EQ(cpu.X, 0xFFFF); // check X is unchanged
    EXPECT_EQ(cpu.PC, 0x00001028); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b00100000); // check only default flag set
    EXPECT_EQ(cpu.cycles, 9);
}

TEST_F(ORATest, InclusiveOrAbsoluteXIndexed_ZEROFLAG) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_ORA_ABS_X);
    cpu.writeWord(0x00001024, 0x00012345);
    cpu.X = 0x0002;
    cpu.writeTwoBytes(0x00012347, 0x0000);
    cpu.A = 0x0000;

    cpu.start(7);

    EXPECT_EQ(cpu.A, 0x0000 | 0x0000); // check accumulator is correctly bitwise inclusive or
    EXPECT_EQ(cpu.X, 0x0002); // check X is unchanged
    EXPECT_EQ(cpu.PC, 0x00001028); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b00100010); // check only default and zero flag set
    EXPECT_EQ(cpu.cycles, 7);
}

TEST_F(ORATest, InclusiveOrAbsoluteXIndexed_NEGATIVEFLAG) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_ORA_ABS_X);
    cpu.writeWord(0x00001024, 0x00012345);
    cpu.X = 0x0002;
    cpu.writeTwoBytes(0x00012347, 0xFF34);
    cpu.A = 0xFF78;

    cpu.start(7);

    EXPECT_EQ(cpu.A, 0xFF34 | 0xFF78); // check accumulator is correctly bitwise inclusive or
    EXPECT_EQ(cpu.X, 0x0002); // check X is unchanged
    EXPECT_EQ(cpu.PC, 0x00001028); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b10100000); // check only default and negative flag set
    EXPECT_EQ(cpu.cycles, 7);
}


TEST_F(ORATest, InclusiveOrAbsoluteYIndexed_NORMAL) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_ORA_ABS_Y);
    cpu.writeWord(0x00001024, 0x00012345);
    cpu.Y = 0x0002;
    cpu.writeTwoBytes(0x00012347, 0x1234);
    cpu.A = 0x5678;

    cpu.start(7);

    EXPECT_EQ(cpu.A, 0x1234 | 0x5678); // check accumulator is correctly bitwise inclusive or
    EXPECT_EQ(cpu.Y, 0x0002); // check Y is unchanged
    EXPECT_EQ(cpu.PC, 0x00001028); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b00100000); // check only default flag set
    EXPECT_EQ(cpu.cycles, 7);
}

TEST_F(ORATest, InclusiveOrAbsoluteYIndexed_PAGECROSSING) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_ORA_ABS_Y);
    cpu.writeWord(0x00001024, 0x00000001);
    cpu.Y = 0xFFFF;
    cpu.writeTwoBytes(0x00010000, 0x1234);
    cpu.A = 0x5678;

    cpu.start(9);

    EXPECT_EQ(cpu.A, 0x1234 | 0x5678); // check accumulator is correctly bitwise inclusive or
    EXPECT_EQ(cpu.Y, 0xFFFF); // check Y is unchanged
    EXPECT_EQ(cpu.PC, 0x00001028); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b00100000); // check only default flag set
    EXPECT_EQ(cpu.cycles, 9);
}

TEST_F(ORATest, InclusiveOrAbsoluteYIndexed_ZEROFLAG) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_ORA_ABS_Y);
    cpu.writeWord(0x00001024, 0x00012345);
    cpu.Y = 0x0002;
    cpu.writeTwoBytes(0x00012347, 0x0000);
    cpu.A = 0x0000;

    cpu.start(7);

    EXPECT_EQ(cpu.A, 0x0000 | 0x0000); // check accumulator is correctly bitwise inclusive or
    EXPECT_EQ(cpu.Y, 0x0002); // check Y is unchanged
    EXPECT_EQ(cpu.PC, 0x00001028); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b00100010); // check only default and zero flag set
    EXPECT_EQ(cpu.cycles, 7);
}

TEST_F(ORATest, InclusiveOrAbsoluteYIndexed_NEGATIVEFLAG) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_ORA_ABS_Y);
    cpu.writeWord(0x00001024, 0x00012345);
    cpu.Y = 0x0002;
    cpu.writeTwoBytes(0x00012347, 0xFF34);
    cpu.A = 0xFF78;

    cpu.start(7);

    EXPECT_EQ(cpu.A, 0xFF34 | 0xFF78); // check accumulator is correctly bitwise inclusive or
    EXPECT_EQ(cpu.Y, 0x0002); // check Y is unchanged
    EXPECT_EQ(cpu.PC, 0x00001028); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b10100000); // check only default and negative flag set
    EXPECT_EQ(cpu.cycles, 7);
}


TEST_F(ORATest, InclusiveOrXIndexedIndirect_NORMAL) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_ORA_X_IND);
    cpu.writeTwoBytes(0x00001024, 0x1234);
    cpu.X = 0x0002;
    cpu.writeWord(0x00001236, 0x00012345);
    cpu.writeTwoBytes(0x00012345, 0x1234);
    cpu.A = 0x5678;

    cpu.start(10);

    EXPECT_EQ(cpu.A, 0x1234 | 0x5678); // check accumulator is correctly bitwise inclusive or
    EXPECT_EQ(cpu.X, 0x0002); // check X is unchanged
    EXPECT_EQ(cpu.PC, 0x00001026); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b00100000); // check only default flag set
    EXPECT_EQ(cpu.cycles, 10);
}

TEST_F(ORATest, InclusiveOrXIndexedIndirect_ZEROFLAG) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_ORA_X_IND);
    cpu.writeTwoBytes(0x00001024, 0x1234);
    cpu.X = 0x0002;
    cpu.writeWord(0x00001236, 0x00012345);
    cpu.writeTwoBytes(0x00012345, 0x0000);
    cpu.A = 0x0000;

    cpu.start(10);

    EXPECT_EQ(cpu.A, 0x0000 | 0x0000); // check accumulator is correctly bitwise inclusive or
    EXPECT_EQ(cpu.X, 0x0002); // check X is unchanged
    EXPECT_EQ(cpu.PC, 0x00001026); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b00100010); // check only default and zero flag set
    EXPECT_EQ(cpu.cycles, 10);
}

TEST_F(ORATest, InclusiveOrXIndexedIndirect_NEGATIVEFLAG) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_ORA_X_IND);
    cpu.writeTwoBytes(0x00001024, 0x1234);
    cpu.X = 0x0002;
    cpu.writeWord(0x00001236, 0x00012345);
    cpu.writeTwoBytes(0x00012345, 0xFF34);
    cpu.A = 0xFF78;

    cpu.start(10);

    EXPECT_EQ(cpu.A, 0xFF34 | 0xFF78); // check accumulator is correctly bitwise inclusive or
    EXPECT_EQ(cpu.X, 0x0002); // check X is unchanged
    EXPECT_EQ(cpu.PC, 0x00001026); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b10100000); // check only default and negative flag set
    EXPECT_EQ(cpu.cycles, 10);
}


TEST_F(ORATest, InclusiveOrIndirectYIndexed_NORMAL) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_ORA_IND_Y);
    cpu.writeTwoBytes(0x00001024, 0x1234);
    cpu.writeWord(0x00001234, 0x00012345);
    cpu.Y = 0x0002;
    cpu.writeTwoBytes(0x00012347, 0x1234);
    cpu.A = 0x5678;

    cpu.start(9);

    EXPECT_EQ(cpu.A, 0x1234 | 0x5678); // check accumulator is correctly bitwise inclusive or
    EXPECT_EQ(cpu.Y, 0x0002); // check Y is unchanged
    EXPECT_EQ(cpu.PC, 0x00001026); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b00100000); // check only default flag set
    EXPECT_EQ(cpu.cycles, 9);
}

TEST_F(ORATest, InclusiveOrIndirectYIndexed_PAGECROSSING) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_ORA_IND_Y);
    cpu.writeTwoBytes(0x00001024, 0x1234);
    cpu.writeWord(0x00001234, 0x00000001);
    cpu.Y = 0xFFFF;
    cpu.writeTwoBytes(0x00010000, 0x1234);
    cpu.A = 0x5678;

    cpu.start(11);

    EXPECT_EQ(cpu.A, 0x1234 | 0x5678); // check accumulator is correctly bitwise inclusive or
    EXPECT_EQ(cpu.Y, 0xFFFF); // check Y is unchanged
    EXPECT_EQ(cpu.PC, 0x00001026); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b00100000); // check only default flag set
    EXPECT_EQ(cpu.cycles, 11);
}

TEST_F(ORATest, InclusiveOrIndirectYIndexed_ZEROFLAG) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_ORA_IND_Y);
    cpu.writeTwoBytes(0x00001024, 0x1234);
    cpu.writeWord(0x00001234, 0x00012345);
    cpu.Y = 0x0002;
    cpu.writeTwoBytes(0x00012347, 0x0000);
    cpu.A = 0x0000;

    cpu.start(9);

    EXPECT_EQ(cpu.A, 0x0000 | 0x0000); // check accumulator is correctly bitwise inclusive or
    EXPECT_EQ(cpu.Y, 0x0002); // check Y is unchanged
    EXPECT_EQ(cpu.PC, 0x00001026); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b00100010); // check only default and zero flag set
    EXPECT_EQ(cpu.cycles, 9);
}

TEST_F(ORATest, InclusiveOrIndirectYIndexed_NEGATIVEFLAG) {
    // setting reset vector to begin processing instructions at 0x00001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_ORA_IND_Y);
    cpu.writeTwoBytes(0x00001024, 0x1234);
    cpu.writeWord(0x00001234, 0x00012345);
    cpu.Y = 0x0002;
    cpu.writeTwoBytes(0x00012347, 0xFF34);
    cpu.A = 0xFF78;

    cpu.start(9);

    EXPECT_EQ(cpu.A, 0xFF34 | 0xFF78); // check accumulator is correctly bitwise inclusive or
    EXPECT_EQ(cpu.Y, 0x0002); // check Y is unchanged
    EXPECT_EQ(cpu.PC, 0x00001026); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b10100000); // check only default and negative flag set
    EXPECT_EQ(cpu.cycles, 9);
}


TEST_F(ORATest, InclusiveOrZeroPage_NORMAL) {
    // setting reset vector to begin processing instructions at 0x00011023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00011023);
    cpu.writeByte(0x00011023, AlienCPU::INS_ORA_ZP);
    cpu.writeTwoBytes(0x00011024, 0x1234);
    cpu.writeTwoBytes(0x00001234, 0x1234);
    cpu.A = 0x5678;

    cpu.start(5);

    EXPECT_EQ(cpu.A, 0x1234 | 0x5678); // check accumulator is correctly bitwise inclusive or
    EXPECT_EQ(cpu.PC, 0x00011026); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b00100000); // check only default flag set
    EXPECT_EQ(cpu.cycles, 5);
}

TEST_F(ORATest, InclusiveOrZeroPage_ZEROFLAG) {
    // setting reset vector to begin processing instructions at 0x00011023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00011023);
    cpu.writeByte(0x00011023, AlienCPU::INS_ORA_ZP);
    cpu.writeTwoBytes(0x00011024, 0x1234);
    cpu.writeTwoBytes(0x00001234, 0x0000);
    cpu.A = 0x0000;

    cpu.start(5);

    EXPECT_EQ(cpu.A, 0x0000 | 0x0000); // check accumulator is correctly bitwise inclusive or
    EXPECT_EQ(cpu.PC, 0x00011026); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b00100010); // check only default and zero flag set
    EXPECT_EQ(cpu.cycles, 5);
}

TEST_F(ORATest, InclusiveOrZeroPage_NEGATIVEFLAG) {
    // setting reset vector to begin processing instructions at 0x00011023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00011023);
    cpu.writeByte(0x00011023, AlienCPU::INS_ORA_ZP);
    cpu.writeTwoBytes(0x00011024, 0x1234);
    cpu.writeTwoBytes(0x00001234, 0xFF34);
    cpu.A = 0xFF78;

    cpu.start(5);

    EXPECT_EQ(cpu.A, 0xFF34 | 0xFF78); // check accumulator is correctly bitwise inclusive or
    EXPECT_EQ(cpu.PC, 0x00011026); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b10100000); // check only default and negative flag set
    EXPECT_EQ(cpu.cycles, 5);
}


TEST_F(ORATest, InclusiveOrZeroPageXIndexed_NORMAL) {
    // setting reset vector to begin processing instructions at 0x00011023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00011023);
    cpu.writeByte(0x00011023, AlienCPU::INS_ORA_ZP_X);
    cpu.writeTwoBytes(0x00011024, 0x1234);
    cpu.X = 0x0002;
    cpu.writeTwoBytes(0x00001236, 0x1234);
    cpu.A = 0x5678;

    cpu.start(6);

    EXPECT_EQ(cpu.A, 0x1234 | 0x5678); // check accumulator is correctly bitwise inclusive or
    EXPECT_EQ(cpu.X, 0x0002); // check X is unchanged
    EXPECT_EQ(cpu.PC, 0x00011026); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b00100000); // check only default flag set
    EXPECT_EQ(cpu.cycles, 6);
}

TEST_F(ORATest, InclusiveOrZeroPageXIndexed_PAGEWRAPPING) {
    // setting reset vector to begin processing instructions at 0x00011023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00011023);
    cpu.writeByte(0x00011023, AlienCPU::INS_ORA_ZP_X);
    cpu.writeTwoBytes(0x00011024, 0x0002);
    cpu.X = 0xFFFF;
    cpu.writeTwoBytes(0x00000001, 0x1234);
    cpu.A = 0x5678;

    cpu.start(6);

    EXPECT_EQ(cpu.A, 0x1234 | 0x5678); // check accumulator is correctly bitwise inclusive or
    EXPECT_EQ(cpu.X, 0xFFFF); // check X is unchanged
    EXPECT_EQ(cpu.PC, 0x00011026); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b00100000); // check only default flag set
    EXPECT_EQ(cpu.cycles, 6);
}

TEST_F(ORATest, InclusiveOrZeroPageXIndexed_ZEROFLAG) {
    // setting reset vector to begin processing instructions at 0x00011023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00011023);
    cpu.writeByte(0x00011023, AlienCPU::INS_ORA_ZP_X);
    cpu.writeTwoBytes(0x00011024, 0x1234);
    cpu.X = 0x0002;
    cpu.writeTwoBytes(0x00001236, 0x0000);
    cpu.A = 0x0000;

    cpu.start(6);

    EXPECT_EQ(cpu.A, 0x0000 | 0x0000); // check accumulator is correctly bitwise inclusive or
    EXPECT_EQ(cpu.X, 0x0002); // check X is unchanged
    EXPECT_EQ(cpu.PC, 0x00011026); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b00100010); // check only default and zero flag set
    EXPECT_EQ(cpu.cycles, 6);
}

TEST_F(ORATest, InclusiveOrZeroPageXIndexed_NEGATIVEFLAG) {
    // setting reset vector to begin processing instructions at 0x00011023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00011023);
    cpu.writeByte(0x00011023, AlienCPU::INS_ORA_ZP_X);
    cpu.writeTwoBytes(0x00011024, 0x1234);
    cpu.X = 0x0002;
    cpu.writeTwoBytes(0x00001236, 0xFF34);
    cpu.A = 0xFF78;

    cpu.start(6);

    EXPECT_EQ(cpu.A, 0xFF34 | 0xFF78); // check accumulator is correctly bitwise inclusive or
    EXPECT_EQ(cpu.X, 0x0002); // check X is unchanged
    EXPECT_EQ(cpu.PC, 0x00011026); // check PC points to the next instruction
    EXPECT_EQ(cpu.P, 0b10100000); // check only default and negative flag set
    EXPECT_EQ(cpu.cycles, 6);
}
