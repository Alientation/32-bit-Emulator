#include <gtest/gtest.h>

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

TEST_F(ADCTest, AddWithCarryImmediate_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_ADC_IMM);
    cpu.writeTwoBytes(0x00001024, 0x2034);
    cpu.A = 0x0012;
    cpu.setFlag(cpu.C_FLAG, true);

    cpu.start(3);

    EXPECT_EQ(cpu.A, 0x2047); // check incremented accumulator value
    EXPECT_EQ(cpu.PC, 0x00001026); // check PC points to next instruction
    EXPECT_EQ(cpu.cycles, 3);
    EXPECT_EQ(cpu.P, 0b00100000); // only default flag is set
}

TEST_F(ADCTest, AddWithCarryImmediate_CARRYFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_ADC_IMM);
    cpu.writeTwoBytes(0x00001024, 0x0001);
    cpu.A = 0xFFFF;

    cpu.start(3);

    EXPECT_EQ(cpu.A, 0x0000); // check incremented accumulator value
    EXPECT_EQ(cpu.PC, 0x00001026); // check PC points to next instruction
    EXPECT_EQ(cpu.cycles, 3);
    EXPECT_EQ(cpu.P, 0b00100011); // only default, zero, and carry flag is set
}


TEST_F(ADCTest, AddWithCarryAbsolute_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_ADC_ABS);
    cpu.writeWord(0x00001024, 0x00012034);
    cpu.writeTwoBytes(0x00012034, 0x1234);
    cpu.A = 0x0012;
    cpu.setFlag(cpu.C_FLAG, true);

    cpu.start(7);

    EXPECT_EQ(cpu.A, 0x1247); // check incremented accumulator value
    EXPECT_EQ(cpu.PC, 0x00001028); // check PC points to next instruction
    EXPECT_EQ(cpu.cycles, 7);
    EXPECT_EQ(cpu.P, 0b00100000); // only default flag is set
}

TEST_F(ADCTest, AddWithCarryAbsolute_CARRYFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_ADC_ABS);
    cpu.writeWord(0x00001024, 0x00012034);
    cpu.writeTwoBytes(0x00012034, 0x0001);
    cpu.A = 0xFFFF;

    cpu.start(7);

    EXPECT_EQ(cpu.A, 0x0000); // check incremented accumulator value
    EXPECT_EQ(cpu.PC, 0x00001028); // check PC points to next instruction
    EXPECT_EQ(cpu.cycles, 7);
    EXPECT_EQ(cpu.P, 0b00100011); // only default, zero, and carry flag is set
}


TEST_F(ADCTest, AddWithCarryAbsoluteXIndexed_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_ADC_ABS_X);
    cpu.writeWord(0x00001024, 0x00012034);
    cpu.X = 0x0001;
    cpu.writeTwoBytes(0x00012035, 0x1234);
    cpu.A = 0x0012;
    cpu.setFlag(cpu.C_FLAG, true);

    cpu.start(7);

    EXPECT_EQ(cpu.A, 0x1247); // check incremented accumulator value
    EXPECT_EQ(cpu.PC, 0x00001028); // check PC points to next instruction
    EXPECT_EQ(cpu.cycles, 7);
    EXPECT_EQ(cpu.P, 0b00100000); // only default flag is set
}

TEST_F(ADCTest, AddWithCarryAbsoluteXIndexed_PAGECROSSING) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_ADC_ABS_X);
    cpu.writeWord(0x00001024, 0x0001FFFF);
    cpu.X = 0x0001;
    cpu.writeTwoBytes(0x00020000, 0x1234);
    cpu.A = 0x0012;
    cpu.setFlag(cpu.C_FLAG, true);

    cpu.start(9);

    EXPECT_EQ(cpu.A, 0x1247); // check incremented accumulator value
    EXPECT_EQ(cpu.PC, 0x00001028); // check PC points to next instruction
    EXPECT_EQ(cpu.cycles, 9);
    EXPECT_EQ(cpu.P, 0b00100000); // only default flag is set
}

TEST_F(ADCTest, AddWithCarryAbsoluteXIndexed_CARRYFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_ADC_ABS_X);
    cpu.writeWord(0x00001024, 0x00012034);
    cpu.X = 0x0001;
    cpu.writeTwoBytes(0x00012035, 0x0001);
    cpu.A = 0xFFFF;

    cpu.start(7);

    EXPECT_EQ(cpu.A, 0x0000); // check incremented accumulator value
    EXPECT_EQ(cpu.PC, 0x00001028); // check PC points to next instruction
    EXPECT_EQ(cpu.cycles, 7);
    EXPECT_EQ(cpu.P, 0b00100011); // only default, zero, and carry flag is set
}


TEST_F(ADCTest, AddWithCarryAbsoluteYIndexed_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_ADC_ABS_Y);
    cpu.writeWord(0x00001024, 0x00012034);
    cpu.Y = 0x0001;
    cpu.writeTwoBytes(0x00012035, 0x1234);
    cpu.A = 0x0012;
    cpu.setFlag(cpu.C_FLAG, true);

    cpu.start(7);

    EXPECT_EQ(cpu.A, 0x1247); // check incremented accumulator value
    EXPECT_EQ(cpu.PC, 0x00001028); // check PC points to next instruction
    EXPECT_EQ(cpu.cycles, 7);
    EXPECT_EQ(cpu.P, 0b00100000); // only default flag is set
}

TEST_F(ADCTest, AddWithCarryAbsoluteYIndexed_PAGECROSSING) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_ADC_ABS_Y);
    cpu.writeWord(0x00001024, 0x0001FFFF);
    cpu.Y = 0x0001;
    cpu.writeTwoBytes(0x00020000, 0x1234);
    cpu.A = 0x0012;
    cpu.setFlag(cpu.C_FLAG, true);

    cpu.start(9);

    EXPECT_EQ(cpu.A, 0x1247); // check incremented accumulator value
    EXPECT_EQ(cpu.Y, 0x0001); // check unchanged Y register
    EXPECT_EQ(cpu.PC, 0x00001028); // check PC points to next instruction
    EXPECT_EQ(cpu.cycles, 9);
    EXPECT_EQ(cpu.P, 0b00100000); // only default flag is set
}

TEST_F(ADCTest, AddWithCarryAbsoluteYIndexed_CARRYFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_ADC_ABS_Y);
    cpu.writeWord(0x00001024, 0x00012034);
    cpu.Y = 0x0001;
    cpu.writeTwoBytes(0x00012035, 0x0001);
    cpu.A = 0xFFFF;

    cpu.start(7);

    EXPECT_EQ(cpu.A, 0x0000); // check incremented accumulator value
    EXPECT_EQ(cpu.Y, 0x0001); // check unchanged Y register
    EXPECT_EQ(cpu.PC, 0x00001028); // check PC points to next instruction
    EXPECT_EQ(cpu.cycles, 7);
    EXPECT_EQ(cpu.P, 0b00100011); // only default, zero, and carry flag is set
}


TEST_F(ADCTest, AddWithCarryXIndexedIndirect_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00000123);
    cpu.writeByte(0x00000123, AlienCPU::INS_ADC_X_IND);
    cpu.writeTwoBytes(0x00000124, 0x1234);
    cpu.X = 0x0001;
    cpu.writeWord(0x00001235, 0x00012345);
    cpu.writeTwoBytes(0x00012345, 0x1234);
    cpu.A = 0x0012;
    cpu.setFlag(cpu.C_FLAG, true);

    cpu.start(10);

    EXPECT_EQ(cpu.A, 0x1247); // check incremented accumulator value
    EXPECT_EQ(cpu.X, 0x0001); // check unchanged X register
    EXPECT_EQ(cpu.PC, 0x00000126); // check PC points to next instruction
    EXPECT_EQ(cpu.cycles, 10);
    EXPECT_EQ(cpu.P, 0b00100000); // only default flag is set
}

TEST_F(ADCTest, AddWithCarryXIndexedIndirect_PAGEWRAP) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00000123);
    cpu.writeByte(0x00000123, AlienCPU::INS_ADC_X_IND);
    cpu.writeTwoBytes(0x00000124, 0x0002);
    cpu.X = 0xFFFF;
    cpu.writeWord(0x00000001, 0x00012345);
    cpu.writeTwoBytes(0x00012345, 0x1234);
    cpu.A = 0x0012;
    cpu.setFlag(cpu.C_FLAG, true);

    cpu.start(10);

    EXPECT_EQ(cpu.A, 0x1247); // check incremented accumulator value
    EXPECT_EQ(cpu.X, 0xFFFF); // check unchanged X register
    EXPECT_EQ(cpu.PC, 0x00000126); // check PC points to next instruction
    EXPECT_EQ(cpu.cycles, 10);
    EXPECT_EQ(cpu.P, 0b00100000); // only default flag is set
}

TEST_F(ADCTest, AddWithCarryXIndexedIndirect_CARRYFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00000123);
    cpu.writeByte(0x00000123, AlienCPU::INS_ADC_X_IND);
    cpu.writeTwoBytes(0x00000124, 0x1234);
    cpu.X = 0x0001;
    cpu.writeWord(0x00001235, 0x00012345);
    cpu.writeTwoBytes(0x00012345, 0x0001);
    cpu.A = 0xFFFF;
    cpu.setFlag(cpu.C_FLAG, true);

    cpu.start(10);

    EXPECT_EQ(cpu.A, 0x0001); // check incremented accumulator value
    EXPECT_EQ(cpu.X, 0x0001); // check unchanged X register
    EXPECT_EQ(cpu.PC, 0x00000126); // check PC points to next instruction
    EXPECT_EQ(cpu.cycles, 10);
    EXPECT_EQ(cpu.P, 0b00100001); // only default and carry flag is set
}


TEST_F(ADCTest, AddWithCarryIndirectYIndexed_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00011234);
    cpu.writeByte(0x00011234, AlienCPU::INS_ADC_IND_Y);
    cpu.writeTwoBytes(0x00011235, 0x1234);
    cpu.writeWord(0x00001234, 0x00012345);
    cpu.Y = 0x0001;
    cpu.writeTwoBytes(0x00012346, 0x1234);
    cpu.A = 0x0012;
    cpu.setFlag(cpu.C_FLAG, true);

    cpu.start(9);

    EXPECT_EQ(cpu.A, 0x1247); // check incremented accumulator value
    EXPECT_EQ(cpu.Y, 0x0001); // check unchanged Y register
    EXPECT_EQ(cpu.PC, 0x00011237); // check PC points to next instruction
    EXPECT_EQ(cpu.cycles, 9);
    EXPECT_EQ(cpu.P, 0b00100000); // only default flag is set
}

TEST_F(ADCTest, AddWithCarryIndirectYIndexed_PAGECROSSING) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00011234);
    cpu.writeByte(0x00011234, AlienCPU::INS_ADC_IND_Y);
    cpu.writeTwoBytes(0x00011235, 0x1234);
    cpu.writeWord(0x00001234, 0x00010001);
    cpu.Y = 0xFFFF;
    cpu.writeTwoBytes(0x00020000, 0x1234);
    cpu.A = 0x0012;
    cpu.setFlag(cpu.C_FLAG, true);

    cpu.start(11);

    EXPECT_EQ(cpu.A, 0x1247); // check incremented accumulator value
    EXPECT_EQ(cpu.Y, 0xFFFF); // check unchanged Y register
    EXPECT_EQ(cpu.PC, 0x00011237); // check PC points to next instruction
    EXPECT_EQ(cpu.cycles, 11);
    EXPECT_EQ(cpu.P, 0b00100000); // only default flag is set
}

TEST_F(ADCTest, AddWithCarryIndirectYIndexed_CARRYFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00011234);
    cpu.writeByte(0x00011234, AlienCPU::INS_ADC_IND_Y);
    cpu.writeTwoBytes(0x00011235, 0x1234);
    cpu.writeWord(0x00001234, 0x00012345);
    cpu.Y = 0x0001;
    cpu.writeTwoBytes(0x00012346, 0x0001);
    cpu.A = 0xFFFF;
    cpu.setFlag(cpu.C_FLAG, true);

    cpu.start(9);

    EXPECT_EQ(cpu.A, 0x0001); // check incremented accumulator value
    EXPECT_EQ(cpu.Y, 0x0001); // check unchanged Y register
    EXPECT_EQ(cpu.PC, 0x00011237); // check PC points to next instruction
    EXPECT_EQ(cpu.cycles, 9);
    EXPECT_EQ(cpu.P, 0b00100001); // only default and carry flag is set
}


TEST_F(ADCTest, AddWithCarryZeroPage_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00011234);
    cpu.writeByte(0x00011234, AlienCPU::INS_ADC_ZP);
    cpu.writeTwoBytes(0x00011235, 0x1234);
    cpu.writeTwoBytes(0x00001234, 0x1234);
    cpu.A = 0x0012;
    cpu.setFlag(cpu.C_FLAG, true);

    cpu.start(5);

    EXPECT_EQ(cpu.A, 0x1247); // check incremented accumulator value
    EXPECT_EQ(cpu.PC, 0x00011237); // check PC points to next instruction
    EXPECT_EQ(cpu.cycles, 5);
    EXPECT_EQ(cpu.P, 0b00100000); // only default flag is set
}

TEST_F(ADCTest, AddWithCarryZeroPage_CARRYFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00011234);
    cpu.writeByte(0x00011234, AlienCPU::INS_ADC_ZP);
    cpu.writeTwoBytes(0x00011235, 0x1234);
    cpu.writeTwoBytes(0x00001234, 0x0001);
    cpu.A = 0xFFFF;
    cpu.setFlag(cpu.C_FLAG, true);

    cpu.start(5);

    EXPECT_EQ(cpu.A, 0x0001); // check incremented accumulator value
    EXPECT_EQ(cpu.PC, 0x00011237); // check PC points to next instruction
    EXPECT_EQ(cpu.cycles, 5);
    EXPECT_EQ(cpu.P, 0b00100001); // only default and carry flag is set
}


TEST_F(ADCTest, AddWithCarryZeroPageXIndexed_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00011234);
    cpu.writeByte(0x00011234, AlienCPU::INS_ADC_ZP_X);
    cpu.writeTwoBytes(0x00011235, 0x1234);
    cpu.X = 0x0001;
    cpu.writeTwoBytes(0x00001235, 0x1234);
    cpu.A = 0x0012;
    cpu.setFlag(cpu.C_FLAG, true);

    cpu.start(6);

    EXPECT_EQ(cpu.A, 0x1247); // check incremented accumulator value
    EXPECT_EQ(cpu.X, 0x0001); // check unchanged X register
    EXPECT_EQ(cpu.PC, 0x00011237); // check PC points to next instruction
    EXPECT_EQ(cpu.cycles, 6);
    EXPECT_EQ(cpu.P, 0b00100000); // only default flag is set
}

TEST_F(ADCTest, AddWithCarryZeroPageXIndexed_PAGEWRAPPING) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00011234);
    cpu.writeByte(0x00011234, AlienCPU::INS_ADC_ZP_X);
    cpu.writeTwoBytes(0x00011235, 0x0001);
    cpu.X = 0xFFFF;
    cpu.writeTwoBytes(0x00000000, 0x1234);
    cpu.A = 0x0012;
    cpu.setFlag(cpu.C_FLAG, true);

    cpu.start(6);

    EXPECT_EQ(cpu.A, 0x1247); // check incremented accumulator value
    EXPECT_EQ(cpu.X, 0xFFFF); // check unchanged X register
    EXPECT_EQ(cpu.PC, 0x00011237); // check PC points to next instruction
    EXPECT_EQ(cpu.cycles, 6);
    EXPECT_EQ(cpu.P, 0b00100000); // only default flag is set
}

TEST_F(ADCTest, AddWithCarryZeroPageXIndexed_CARRYFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00011234);
    cpu.writeByte(0x00011234, AlienCPU::INS_ADC_ZP_X);
    cpu.writeTwoBytes(0x00011235, 0x1234);
    cpu.X = 0x0001;
    cpu.writeTwoBytes(0x00001235, 0x0001);
    cpu.A = 0xFFFF;
    cpu.setFlag(cpu.C_FLAG, true);

    cpu.start(6);

    EXPECT_EQ(cpu.A, 0x0001); // check incremented accumulator value
    EXPECT_EQ(cpu.X, 0x0001); // check unchanged X register
    EXPECT_EQ(cpu.PC, 0x00011237); // check PC points to next instruction
    EXPECT_EQ(cpu.cycles, 6);
    EXPECT_EQ(cpu.P, 0b00100001); // only default and carry flag is set
}