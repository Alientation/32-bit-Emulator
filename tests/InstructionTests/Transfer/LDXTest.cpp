#include <gtest/gtest.h>

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
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDX_IMM);

    // write value to load into X
    cpu.writeTwoBytes(0x00001024, 0x4232);

    cpu.start(3);

    // test X is set to the correct high endian value
    EXPECT_EQ(cpu.X, 0x4232);

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 3);
}

TEST_F(LDXTest, LoadX_Immediate_ZFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDX_IMM);

    // write value to load into X
    cpu.writeTwoBytes(0x00001024, 0x0000);

    cpu.start(3);

    // test X is set to the correct high endian value
    EXPECT_EQ(cpu.X, 0x0000);

    // test zero and default flags are set
    EXPECT_EQ(cpu.P, 0b00100010);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 3);
}

TEST_F(LDXTest, LoadX_Immediate_NFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDX_IMM);

    // write value to load into X
    cpu.writeTwoBytes(0x00001024, 0xFFEF);

    cpu.start(3);

    // test X is set to the correct high endian value
    EXPECT_EQ(cpu.X, 0xFFEF);

    // test negative and default flags are set
    EXPECT_EQ(cpu.P, 0b10100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 3);
}


// LDX ABSOLUTE TESTS
TEST_F(LDXTest, LoadX_Absolute_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDX_ABS);

    // write memory address of value to load into X
    cpu.writeWord(0x00001024, 0x00014232);

    // write value to load into X
    cpu.writeTwoBytes(0x00014232, 0x1234);

    cpu.start(7);

    // test X is set to the correct high endian value
    EXPECT_EQ(cpu.X, 0x1234);

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001028);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 7);
}

TEST_F(LDXTest, LoadX_Absolute_ZFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDX_ABS);

    // write memory address of value to load into X
    cpu.writeWord(0x00001024, 0x00014232);

    // write value to load into X
    cpu.writeTwoBytes(0x00014232, 0x0000);

    cpu.start(7);

    // test X is set to the correct high endian value
    EXPECT_EQ(cpu.X, 0x0000);

    // test zero and default flag is set
    EXPECT_EQ(cpu.P, 0b00100010);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001028);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 7);
}

TEST_F(LDXTest, LoadX_Absolute_NFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDX_ABS);

    // write memory address of value to load into X
    cpu.writeWord(0x00001024, 0x00014232);

    // write value to load into X
    cpu.writeTwoBytes(0x00014232, 0xFFEF);

    cpu.start(7);

    // test X is set to the correct high endian value
    EXPECT_EQ(cpu.X, 0xFFEF);

    // test negative and default flags are set
    EXPECT_EQ(cpu.P, 0b10100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001028);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 7);
}


// LDX ABSOLUTE Y-INDEXED TESTS
TEST_F(LDXTest, LoadX_Absolute_YIndexed_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDX_ABS_Y);

    // write memory address of value to load into X
    cpu.writeWord(0x00001024, 0x00014232);
    cpu.Y = 0x0013;

    // write value to load into X
    cpu.writeTwoBytes(0x00014245, 0x1234);

    cpu.start(7);

    // test X is set to the correct high endian value
    EXPECT_EQ(cpu.X, 0x1234);

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001028);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 7);
}

TEST_F(LDXTest, LoadX_Absolute_YIndexed_PAGECROSS) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDX_ABS_Y);

    // write memory address of value to load into X
    cpu.writeWord(0x00001024, 0x00011232);
    cpu.Y = 0xF013;

    // write value to load into X
    cpu.writeTwoBytes(0x00020245, 0x1234);

    cpu.start(9);

    // test X is set to the correct high endian value
    EXPECT_EQ(cpu.X, 0x1234);

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001028);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 9);
}

TEST_F(LDXTest, LoadX_Absolute_YIndexed_ZFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDX_ABS_Y);

    // write memory address of value to load into X
    cpu.writeWord(0x00001024, 0x00014232);
    cpu.Y = 0x0013;

    // write value to load into X
    cpu.writeTwoBytes(0x00014245, 0x0000);

    cpu.start(7);

    // test X is set to the correct high endian value
    EXPECT_EQ(cpu.X, 0x0000);

    // test zero and default flag is set
    EXPECT_EQ(cpu.P, 0b00100010);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001028);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 7);
}

TEST_F(LDXTest, LoadX_Absolute_YIndexed_NFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDX_ABS_Y);

    // write memory address of value to load into X
    cpu.writeWord(0x00001024, 0x00014232);
    cpu.Y = 0x0013;

    // write value to load into X
    cpu.writeTwoBytes(0x00014245, 0xFFEF);

    cpu.start(7);

    // test X is set to the correct high endian value
    EXPECT_EQ(cpu.X, 0xFFEF);

    // test negative and default flags are set
    EXPECT_EQ(cpu.P, 0b10100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001028);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 7);
}


// LDX ZERO PAGE TESTS
TEST_F(LDXTest, LoadX_ZeroPage_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDX_ZP);

    // write zero page memory address to the value to load into X
    cpu.writeTwoBytes(0x00001024, 0x4232);

    // write the value to be loaded to the X on the zero page
    cpu.writeTwoBytes(0x00004232, 0x2042);

    cpu.start(5);

    // test X is set to the correct high endian value
    EXPECT_EQ(cpu.X, 0x2042);

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 5);
}

TEST_F(LDXTest, LoadX_ZeroPage_ZFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDX_ZP);

    // write zero page memory address to the value to load into X
    cpu.writeTwoBytes(0x00001024, 0x4232);

    // write the values to be loaded to the X on the zero page
    cpu.writeTwoBytes(0x00004232, 0x0000);

    cpu.start(5);

    // test X is set to the correct high endian value
    EXPECT_EQ(cpu.X, 0x0000);

    // test zero and default flags are set
    EXPECT_EQ(cpu.P, 0b00100010);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 5);
}

TEST_F(LDXTest, LoadX_ZeroPage_NFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDX_ZP);

    // write zero page memory address to the value to load into X
    cpu.writeTwoBytes(0x00001024, 0x4232);

    // write the values to be loaded to the X on the zero page
    cpu.writeTwoBytes(0x00004232, 0xFFEF);

    cpu.start(5);

    // test X is set to the correct high endian value
    EXPECT_EQ(cpu.X, 0xFFEF);

    // test negative and default flags are set
    EXPECT_EQ(cpu.P, 0b10100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 5);
}


// LDX ZEROPAGE Y-INDEXED TESTS
TEST_F(LDXTest, LoadX_ZeroPage_YIndexed_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDX_ZP_Y);

    // write zero page memory address to the value to load into X
    cpu.writeTwoBytes(0x00001024, 0x4232);
    cpu.Y = 0x0013;

    // write the value to be loaded to the X on the zero page
    cpu.writeTwoBytes(0x00004245, 0x2042);

    cpu.start(6);

    // test X is set to the correct high endian value
    EXPECT_EQ(cpu.X, 0x2042);

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 6);
}

TEST_F(LDXTest, LoadX_ZeroPage_YIndexed_WRAPAROUND) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDX_ZP_Y);

    // write zero page memory address to the value to load into X
    cpu.writeTwoBytes(0x00001024, 0x1232);
    cpu.Y = 0xF013;

    // write the value to be loaded to the X on the zero page
    cpu.writeTwoBytes(0x00000245, 0x2042);

    cpu.start(6);

    // test X is set to the correct high endian value
    EXPECT_EQ(cpu.X, 0x2042);

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 6);
}

TEST_F(LDXTest, LoadX_ZeroPage_YIndexed_ZFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDX_ZP_Y);

    // write zero page memory address to the value to load into X
    cpu.writeTwoBytes(0x00001024, 0x4232);
    cpu.Y = 0x0013;

    // write the values to be loaded to the X on the zero page
    cpu.writeTwoBytes(0x00004245, 0x0000);

    cpu.start(6);

    // test X is set to the correct high endian value
    EXPECT_EQ(cpu.X, 0x0000);

    // test zero and default flags are set
    EXPECT_EQ(cpu.P, 0b00100010);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 6);
}

TEST_F(LDXTest, LoadX_ZeroPage_YIndexed_NFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDX_ZP_Y);

    // write zero page memory address to the value to load into X
    cpu.writeTwoBytes(0x00001024, 0x4232);
    cpu.Y = 0x0013;

    // write the values to be loaded to the X on the zero page
    cpu.writeTwoBytes(0x00004245, 0xFFEF);

    cpu.start(6);

    // test X is set to the correct high endian value
    EXPECT_EQ(cpu.X, 0xFFEF);

    // test negative and default flags are set
    EXPECT_EQ(cpu.P, 0b10100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 6);
}