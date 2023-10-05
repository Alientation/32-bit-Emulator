#include <gtest/gtest.h>

#include <AlienCPUTest.h>

class LDYTest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.Reset();
    }

    virtual void TearDown() {

    }
};

// LDX IMMEDIATE TESTS
TEST_F(LDYTest, LoadY_Immediate_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.WriteWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.WriteByte(0x00001023, AlienCPU::INS_LDY_IMM);

    // write value to load into Y
    cpu.WriteTwoBytes(0x00001024, 0x4232);

    cpu.Start(3);

    // test Y is set to the correct high endian value
    EXPECT_EQ(cpu.Y, 0x4232);

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 3);
}

TEST_F(LDYTest, LoadY_Immediate_ZFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.WriteWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.WriteByte(0x00001023, AlienCPU::INS_LDY_IMM);

    // write value to load into Y
    cpu.WriteTwoBytes(0x00001024, 0x0000);

    cpu.Start(3);

    // test Y is set to the correct high endian value
    EXPECT_EQ(cpu.Y, 0x0000);

    // test zero and default flags are set
    EXPECT_EQ(cpu.P, 0b00100010);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 3);
}

TEST_F(LDYTest, LoadY_Immediate_NFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.WriteWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.WriteByte(0x00001023, AlienCPU::INS_LDY_IMM);

    // write value to load into Y
    cpu.WriteTwoBytes(0x00001024, 0xFFEF);

    cpu.Start(3);

    // test Y is set to the correct high endian value
    EXPECT_EQ(cpu.Y, 0xFFEF);

    // test negative and default flags are set
    EXPECT_EQ(cpu.P, 0b10100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 3);
}


// LDX ABSOLUTE TESTS
TEST_F(LDYTest, LoadY_Absolute_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.WriteWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.WriteByte(0x00001023, AlienCPU::INS_LDY_ABS);

    // write memory address of value to load into Y
    cpu.WriteWord(0x00001024, 0x00014232);

    // write value to load into Y
    cpu.WriteTwoBytes(0x00014232, 0x1234);

    cpu.Start(7);

    // test Y is set to the correct high endian value
    EXPECT_EQ(cpu.Y, 0x1234);

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001028);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 7);
}

TEST_F(LDYTest, LoadY_Absolute_ZFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.WriteWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.WriteByte(0x00001023, AlienCPU::INS_LDY_ABS);

    // write memory address of value to load into Y
    cpu.WriteWord(0x00001024, 0x00014232);

    // write value to load into Y
    cpu.WriteTwoBytes(0x00014232, 0x0000);

    cpu.Start(7);

    // test Y is set to the correct high endian value
    EXPECT_EQ(cpu.Y, 0x0000);

    // test zero and default flag is set
    EXPECT_EQ(cpu.P, 0b00100010);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001028);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 7);
}

TEST_F(LDYTest, LoadY_Absolute_NFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.WriteWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.WriteByte(0x00001023, AlienCPU::INS_LDY_ABS);

    // write memory address of value to load into Y
    cpu.WriteWord(0x00001024, 0x00014232);

    // write value to load into Y
    cpu.WriteTwoBytes(0x00014232, 0xFFEF);

    cpu.Start(7);

    // test Y is set to the correct high endian value
    EXPECT_EQ(cpu.Y, 0xFFEF);

    // test negative and default flags are set
    EXPECT_EQ(cpu.P, 0b10100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001028);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 7);
}


// LDX ABSOLUTE Y-INDEXED TESTS
TEST_F(LDYTest, LoadY_Absolute_XIndexed_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.WriteWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.WriteByte(0x00001023, AlienCPU::INS_LDY_ABS_X);

    // write memory address of value to load into Y
    cpu.WriteWord(0x00001024, 0x00014232);
    cpu.X = 0x0013;

    // write value to load into Y
    cpu.WriteTwoBytes(0x00014245, 0x1234);

    cpu.Start(7);

    // test Y is set to the correct high endian value
    EXPECT_EQ(cpu.Y, 0x1234);

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001028);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 7);
}

TEST_F(LDYTest, LoadY_Absolute_XIndexed_PAGECROSS) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.WriteWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.WriteByte(0x00001023, AlienCPU::INS_LDY_ABS_X);

    // write memory address of value to load into Y
    cpu.WriteWord(0x00001024, 0x00011232);
    cpu.X = 0xF013;

    // write value to load into Y
    cpu.WriteTwoBytes(0x00020245, 0x1234);

    cpu.Start(9);

    // test Y is set to the correct high endian value
    EXPECT_EQ(cpu.Y, 0x1234);

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001028);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 9);
}

TEST_F(LDYTest, LoadY_Absolute_XIndexed_ZFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.WriteWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.WriteByte(0x00001023, AlienCPU::INS_LDY_ABS_X);

    // write memory address of value to load into Y
    cpu.WriteWord(0x00001024, 0x00014232);
    cpu.X = 0x0013;

    // write value to load into Y
    cpu.WriteTwoBytes(0x00014245, 0x0000);

    cpu.Start(7);

    // test Y is set to the correct high endian value
    EXPECT_EQ(cpu.Y, 0x0000);

    // test zero and default flag is set
    EXPECT_EQ(cpu.P, 0b00100010);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001028);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 7);
}

TEST_F(LDYTest, LoadY_Absolute_XIndexed_NFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.WriteWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.WriteByte(0x00001023, AlienCPU::INS_LDY_ABS_X);

    // write memory address of value to load into Y
    cpu.WriteWord(0x00001024, 0x00014232);
    cpu.X = 0x0013;

    // write value to load into Y
    cpu.WriteTwoBytes(0x00014245, 0xFFEF);

    cpu.Start(7);

    // test Y is set to the correct high endian value
    EXPECT_EQ(cpu.Y, 0xFFEF);

    // test negative and default flags are set
    EXPECT_EQ(cpu.P, 0b10100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001028);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 7);
}


// LDX ZERO PAGE TESTS
TEST_F(LDYTest, LoadY_ZeroPage_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.WriteWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.WriteByte(0x00001023, AlienCPU::INS_LDY_ZP);

    // write zero page memory address to the value to load into Y
    cpu.WriteTwoBytes(0x00001024, 0x4232);

    // write the value to be loaded to the X on the zero page
    cpu.WriteTwoBytes(0x00004232, 0x2042);

    cpu.Start(5);

    // test Y is set to the correct high endian value
    EXPECT_EQ(cpu.Y, 0x2042);

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 5);
}

TEST_F(LDYTest, LoadY_ZeroPage_ZFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.WriteWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.WriteByte(0x00001023, AlienCPU::INS_LDY_ZP);

    // write zero page memory address to the value to load into Y
    cpu.WriteTwoBytes(0x00001024, 0x4232);

    // write the values to be loaded to the X on the zero page
    cpu.WriteTwoBytes(0x00004232, 0x0000);

    cpu.Start(5);

    // test Y is set to the correct high endian value
    EXPECT_EQ(cpu.Y, 0x0000);

    // test zero and default flags are set
    EXPECT_EQ(cpu.P, 0b00100010);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 5);
}

TEST_F(LDYTest, LoadY_ZeroPage_NFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.WriteWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.WriteByte(0x00001023, AlienCPU::INS_LDY_ZP);

    // write zero page memory address to the value to load into Y
    cpu.WriteTwoBytes(0x00001024, 0x4232);

    // write the values to be loaded to the X on the zero page
    cpu.WriteTwoBytes(0x00004232, 0xFFEF);

    cpu.Start(5);

    // test Y is set to the correct high endian value
    EXPECT_EQ(cpu.Y, 0xFFEF);

    // test negative and default flags are set
    EXPECT_EQ(cpu.P, 0b10100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 5);
}


// LDX ZEROPAGE Y-INDEXED TESTS
TEST_F(LDYTest, LoadY_ZeroPage_XIndexed_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.WriteWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.WriteByte(0x00001023, AlienCPU::INS_LDY_ZP_X);

    // write zero page memory address to the value to load into Y
    cpu.WriteTwoBytes(0x00001024, 0x4232);
    cpu.X = 0x0013;

    // write the value to be loaded to the X on the zero page
    cpu.WriteTwoBytes(0x00004245, 0x2042);

    cpu.Start(6);

    // test Y is set to the correct high endian value
    EXPECT_EQ(cpu.Y, 0x2042);

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 6);
}

TEST_F(LDYTest, LoadY_ZeroPage_XIndexed_WRAPAROUND) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.WriteWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.WriteByte(0x00001023, AlienCPU::INS_LDY_ZP_X);

    // write zero page memory address to the value to load into Y
    cpu.WriteTwoBytes(0x00001024, 0x1232);
    cpu.X = 0xF013;

    // write the value to be loaded to the X on the zero page
    cpu.WriteTwoBytes(0x00000245, 0x2042);

    cpu.Start(6);

    // test Y is set to the correct high endian value
    EXPECT_EQ(cpu.Y, 0x2042);

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 6);
}

TEST_F(LDYTest, LoadY_ZeroPage_XIndexed_ZFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.WriteWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.WriteByte(0x00001023, AlienCPU::INS_LDY_ZP_X);

    // write zero page memory address to the value to load into Y
    cpu.WriteTwoBytes(0x00001024, 0x4232);
    cpu.X = 0x0013;

    // write the values to be loaded to the X on the zero page
    cpu.WriteTwoBytes(0x00004245, 0x0000);

    cpu.Start(6);

    // test Y is set to the correct high endian value
    EXPECT_EQ(cpu.Y, 0x0000);

    // test zero and default flags are set
    EXPECT_EQ(cpu.P, 0b00100010);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 6);
}

TEST_F(LDYTest, LoadY_ZeroPage_XIndexed_NFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.WriteWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.WriteByte(0x00001023, AlienCPU::INS_LDY_ZP_X);

    // write zero page memory address to the value to load into Y
    cpu.WriteTwoBytes(0x00001024, 0x4232);
    cpu.X = 0x0013;

    // write the values to be loaded to the X on the zero page
    cpu.WriteTwoBytes(0x00004245, 0xFFEF);

    cpu.Start(6);

    // test Y is set to the correct high endian value
    EXPECT_EQ(cpu.Y, 0xFFEF);

    // test negative and default flags are set
    EXPECT_EQ(cpu.P, 0b10100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 6);
}