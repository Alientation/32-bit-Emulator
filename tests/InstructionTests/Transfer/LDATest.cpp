#include <gtest/gtest.h>

#include <AlienCPUTest.h>

class LDATest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};

// LDA IMMEDIATE TESTS
TEST_F(LDATest, LoadAccumulator_Immediate_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDA_IMM);

    // write value to load into accumulator
    cpu.writeTwoBytes(0x00001024, 0x4232);

    cpu.start(3);

    // test accumulator is set to the correct high endian value
    EXPECT_EQ(cpu.A, 0x4232);

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 3);
}

TEST_F(LDATest, LoadAccumulator_Immediate_ZFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDA_IMM);

    // write value to load into accumulator
    cpu.writeTwoBytes(0x00001024, 0x0000);

    cpu.start(3);

    // test accumulator is set to the correct high endian value
    EXPECT_EQ(cpu.A, 0x0000);

    // test zero and default flags are set
    EXPECT_EQ(cpu.P, 0b00100010);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 3);
}

TEST_F(LDATest, LoadAccumulator_Immediate_NFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDA_IMM);

    // write value to load into accumulator
    cpu.writeTwoBytes(0x00001024, 0xFFEF);

    cpu.start(3);

    // test accumulator is set to the correct high endian value
    EXPECT_EQ(cpu.A, 0xFFEF);

    // test negative and default flags are set
    EXPECT_EQ(cpu.P, 0b10100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 3);
}


// LDA ABSOLUTE TESTS
TEST_F(LDATest, LoadAccumulator_Absolute_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDA_ABS);

    // write memory address of value to load into accumulator
    cpu.writeWord(0x00001024, 0x00014232);

    // write value to load into accumulator
    cpu.writeTwoBytes(0x00014232, 0x1234);

    cpu.start(7);

    // test accumulator is set to the correct high endian value
    EXPECT_EQ(cpu.A, 0x1234);

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001028);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 7);
}

TEST_F(LDATest, LoadAccumulator_Absolute_ZFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDA_ABS);

    // write memory address of value to load into accumulator
    cpu.writeWord(0x00001024, 0x00014232);

    // write value to load into accumulator
    cpu.writeTwoBytes(0x00014232, 0x0000);

    cpu.start(7);

    // test accumulator is set to the correct high endian value
    EXPECT_EQ(cpu.A, 0x0000);

    // test zero and default flag is set
    EXPECT_EQ(cpu.P, 0b00100010);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001028);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 7);
}

TEST_F(LDATest, LoadAccumulator_Absolute_NFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDA_ABS);

    // write memory address of value to load into accumulator
    cpu.writeWord(0x00001024, 0x00014232);

    // write value to load into accumulator
    cpu.writeTwoBytes(0x00014232, 0xFFEF);

    cpu.start(7);

    // test accumulator is set to the correct high endian value
    EXPECT_EQ(cpu.A, 0xFFEF);

    // test negative and default flags are set
    EXPECT_EQ(cpu.P, 0b10100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001028);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 7);
}


// LDA ABSOLUTE X-INDEXED TESTS
TEST_F(LDATest, LoadAccumulator_Absolute_XIndexed_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDA_ABS_X);

    // write memory address of value to load into accumulator
    cpu.writeWord(0x00001024, 0x00014232);
    cpu.X = 0x0013;

    // write value to load into accumulator
    cpu.writeTwoBytes(0x00014245, 0x1234);

    cpu.start(7);

    // test accumulator is set to the correct high endian value
    EXPECT_EQ(cpu.A, 0x1234);

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001028);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 7);
}

TEST_F(LDATest, LoadAccumulator_Absolute_XIndexed_PAGECROSS) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDA_ABS_X);

    // write memory address of value to load into accumulator
    cpu.writeWord(0x00001024, 0x00011232);
    cpu.X = 0xF013;

    // write value to load into accumulator
    cpu.writeTwoBytes(0x00020245, 0x1234);

    cpu.start(9);

    // test accumulator is set to the correct high endian value
    EXPECT_EQ(cpu.A, 0x1234);

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001028);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 9);
}

TEST_F(LDATest, LoadAccumulator_Absolute_XIndexed_ZFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDA_ABS_X);

    // write memory address of value to load into accumulator
    cpu.writeWord(0x00001024, 0x00014232);
    cpu.X = 0x0013;

    // write value to load into accumulator
    cpu.writeTwoBytes(0x00014245, 0x0000);

    cpu.start(7);

    // test accumulator is set to the correct high endian value
    EXPECT_EQ(cpu.A, 0x0000);

    // test zero and default flag is set
    EXPECT_EQ(cpu.P, 0b00100010);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001028);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 7);
}

TEST_F(LDATest, LoadAccumulator_Absolute_XIndexed_NFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDA_ABS_X);

    // write memory address of value to load into accumulator
    cpu.writeWord(0x00001024, 0x00014232);
    cpu.X = 0x0013;

    // write value to load into accumulator
    cpu.writeTwoBytes(0x00014245, 0xFFEF);

    cpu.start(7);

    // test accumulator is set to the correct high endian value
    EXPECT_EQ(cpu.A, 0xFFEF);

    // test negative and default flags are set
    EXPECT_EQ(cpu.P, 0b10100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001028);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 7);
}


// LDA ABSOLUTE Y-INDEXED TESTS
TEST_F(LDATest, LoadAccumulator_Absolute_YIndexed_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDA_ABS_Y);

    // write memory address of value to load into accumulator
    cpu.writeWord(0x00001024, 0x00014232);
    cpu.Y = 0x0013;

    // write value to load into accumulator
    cpu.writeTwoBytes(0x00014245, 0x1234);

    cpu.start(7);

    // test accumulator is set to the correct high endian value
    EXPECT_EQ(cpu.A, 0x1234);

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001028);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 7);
}

TEST_F(LDATest, LoadAccumulator_Absolute_YIndexed_PAGECROSS) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDA_ABS_Y);

    // write memory address of value to load into accumulator
    cpu.writeWord(0x00001024, 0x00011232);
    cpu.Y = 0xF013;

    // write value to load into accumulator
    cpu.writeTwoBytes(0x00020245, 0x1234);

    cpu.start(9);

    // test accumulator is set to the correct high endian value
    EXPECT_EQ(cpu.A, 0x1234);

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001028);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 9);
}

TEST_F(LDATest, LoadAccumulator_Absolute_YIndexed_ZFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDA_ABS_Y);

    // write memory address of value to load into accumulator
    cpu.writeWord(0x00001024, 0x00014232);
    cpu.Y = 0x0013;

    // write value to load into accumulator
    cpu.writeTwoBytes(0x00014245, 0x0000);

    cpu.start(7);

    // test accumulator is set to the correct high endian value
    EXPECT_EQ(cpu.A, 0x0000);

    // test zero and default flag is set
    EXPECT_EQ(cpu.P, 0b00100010);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001028);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 7);
}

TEST_F(LDATest, LoadAccumulator_Absolute_YIndexed_NFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDA_ABS_Y);

    // write memory address of value to load into accumulator
    cpu.writeWord(0x00001024, 0x00014232);
    cpu.Y = 0x0013;

    // write value to load into accumulator
    cpu.writeTwoBytes(0x00014245, 0xFFEF);

    cpu.start(7);

    // test accumulator is set to the correct high endian value
    EXPECT_EQ(cpu.A, 0xFFEF);

    // test negative and default flags are set
    EXPECT_EQ(cpu.P, 0b10100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001028);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 7);
}


// LDA X-INDEXED INDIRECT TESTS
TEST_F(LDATest, LoadAccumulator_XIndexed_Indirect_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDA_X_IND);

    // write zero page memory address of value to load into accumulator
    cpu.writeTwoBytes(0x00001024, 0x4232);
    cpu.X = 0x0013;

    // write memory address of value to load into accumulator
    cpu.writeWord(0x00004245, 0x00011234);

    // write value to load into accumulator
    cpu.writeTwoBytes(0x00011234, 0x1234);

    cpu.start(10);

    // test accumulator is set to the correct high endian value
    EXPECT_EQ(cpu.A, 0x1234);

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 10);
}

TEST_F(LDATest, LoadAccumulator_XIndexed_Indirect_WRAPAROUND) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDA_X_IND);

    // write zero page memory address of value to load into accumulator
    cpu.writeTwoBytes(0x00001024, 0x1232);
    cpu.X = 0xF013;

    // write memory address of value to load into accumulator
    cpu.writeWord(0x00000245, 0x00011234);

    // write value to load into accumulator
    cpu.writeTwoBytes(0x00011234, 0x1234);

    cpu.start(10);

    // test accumulator is set to the correct high endian value
    EXPECT_EQ(cpu.A, 0x1234);

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 10);
}

TEST_F(LDATest, LoadAccumulator_XIndexed_Indirect_ZFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDA_X_IND);

    // write zero page memory address of value to load into accumulator
    cpu.writeTwoBytes(0x00001024, 0x4232);
    cpu.X = 0x0013;

    // write memory address of value to load into accumulator
    cpu.writeWord(0x00004245, 0x00011234);

    // write value to load into accumulator
    cpu.writeTwoBytes(0x00011234, 0x0000);

    cpu.start(10);

    // test accumulator is set to the correct high endian value
    EXPECT_EQ(cpu.A, 0x0000);

    // test zero and default flag is set
    EXPECT_EQ(cpu.P, 0b00100010);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 10);
}

TEST_F(LDATest, LoadAccumulator_XIndexed_Indirect_NFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDA_X_IND);

    // write zero page memory address of value to load into accumulator
    cpu.writeTwoBytes(0x00001024, 0x4232);
    cpu.X = 0x0013;

    // write memory address of value to load into accumulator
    cpu.writeWord(0x00004245, 0x00011234);

    // write value to load into accumulator
    cpu.writeTwoBytes(0x00011234, 0xFFEF);

    cpu.start(10);

    // test accumulator is set to the correct high endian value
    EXPECT_EQ(cpu.A, 0xFFEF);

    // test negative and default flags are set
    EXPECT_EQ(cpu.P, 0b10100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 10);
}


// LDA INDIRECT Y-INDEXED TESTS
TEST_F(LDATest, LoadAccumulator_Indirect_YIndexed_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDA_IND_Y);

    // write zero page memory address of value to load into accumulator
    cpu.writeTwoBytes(0x00001024, 0x4232);

    // write memory address of value to load into accumulator
    cpu.writeWord(0x00004232, 0x00011234);
    cpu.Y = 0x0013;

    // write value to load into accumulator
    cpu.writeTwoBytes(0x00011247, 0x1234);

    cpu.start(9);

    // test accumulator is set to the correct high endian value
    EXPECT_EQ(cpu.A, 0x1234);

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 9);
}

TEST_F(LDATest, LoadAccumulator_Indirect_YIndexed_PAGECROSS) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDA_IND_Y);

    // write zero page memory address of value to load into accumulator
    cpu.writeTwoBytes(0x00001024, 0x4232);

    // write memory address of value to load into accumulator
    cpu.writeWord(0x00004232, 0x00011234);
    cpu.Y = 0xF013;

    // write value to load into accumulator
    cpu.writeTwoBytes(0x00020247, 0x1234);

    cpu.start(11);

    // test accumulator is set to the correct high endian value
    EXPECT_EQ(cpu.A, 0x1234);

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 11);
}

TEST_F(LDATest, LoadAccumulator_Indirect_YIndexed_ZFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDA_IND_Y);

    // write zero page memory address of value to load into accumulator
    cpu.writeTwoBytes(0x00001024, 0x4232);

    // write memory address of value to load into accumulator
    cpu.writeWord(0x00004232, 0x00011234);
    cpu.Y = 0x0013;

    // write value to load into accumulator
    cpu.writeTwoBytes(0x00011247, 0x0000);

    cpu.start(9);

    // test accumulator is set to the correct high endian value
    EXPECT_EQ(cpu.A, 0x0000);

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100010);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 9);
}

TEST_F(LDATest, LoadAccumulator_Indirect_YIndexed_NFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDA_IND_Y);

    // write zero page memory address of value to load into accumulator
    cpu.writeTwoBytes(0x00001024, 0x4232);

    // write memory address of value to load into accumulator
    cpu.writeWord(0x00004232, 0x00011234);
    cpu.Y = 0x0013;

    // write value to load into accumulator
    cpu.writeTwoBytes(0x00011247, 0xFFEF);

    cpu.start(9);

    // test accumulator is set to the correct high endian value
    EXPECT_EQ(cpu.A, 0xFFEF);

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b10100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 9);
}


// LDA ZERO PAGE TESTS
TEST_F(LDATest, LoadAccumulator_ZeroPage_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDA_ZP);

    // write zero page memory address to the value to load into accumulator
    cpu.writeTwoBytes(0x00001024, 0x4232);

    // write the value to be loaded to the accumulator on the zero page
    cpu.writeTwoBytes(0x00004232, 0x2042);

    cpu.start(5);

    // test accumulator is set to the correct high endian value
    EXPECT_EQ(cpu.A, 0x2042);

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 5);
}

TEST_F(LDATest, LoadAccumulator_ZeroPage_ZFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDA_ZP);

    // write zero page memory address to the value to load into accumulator
    cpu.writeTwoBytes(0x00001024, 0x4232);

    // write the values to be loaded to the accumulator on the zero page
    cpu.writeTwoBytes(0x00004232, 0x0000);

    cpu.start(5);

    // test accumulator is set to the correct high endian value
    EXPECT_EQ(cpu.A, 0x0000);

    // test zero and default flags are set
    EXPECT_EQ(cpu.P, 0b00100010);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 5);
}

TEST_F(LDATest, LoadAccumulator_ZeroPage_NFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDA_ZP);

    // write zero page memory address to the value to load into accumulator
    cpu.writeTwoBytes(0x00001024, 0x4232);

    // write the values to be loaded to the accumulator on the zero page
    cpu.writeTwoBytes(0x00004232, 0xFFEF);

    cpu.start(5);

    // test accumulator is set to the correct high endian value
    EXPECT_EQ(cpu.A, 0xFFEF);

    // test negative and default flags are set
    EXPECT_EQ(cpu.P, 0b10100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 5);
}


// LDA ZEROPAGE X-INDEXED TESTS
TEST_F(LDATest, LoadAccumulator_ZeroPage_XIndexed_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDA_ZP_X);

    // write zero page memory address to the value to load into accumulator
    cpu.writeTwoBytes(0x00001024, 0x4232);
    cpu.X = 0x0013;

    // write the value to be loaded to the accumulator on the zero page
    cpu.writeTwoBytes(0x00004245, 0x2042);

    cpu.start(6);

    // test accumulator is set to the correct high endian value
    EXPECT_EQ(cpu.A, 0x2042);

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 6);
}

TEST_F(LDATest, LoadAccumulator_ZeroPage_XIndexed_WRAPAROUND) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDA_ZP_X);

    // write zero page memory address to the value to load into accumulator
    cpu.writeTwoBytes(0x00001024, 0x1232);
    cpu.X = 0xF013;

    // write the value to be loaded to the accumulator on the zero page
    cpu.writeTwoBytes(0x00000245, 0x2042);

    cpu.start(6);

    // test accumulator is set to the correct high endian value
    EXPECT_EQ(cpu.A, 0x2042);

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 6);
}

TEST_F(LDATest, LoadAccumulator_ZeroPage_XIndexed_ZFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDA_ZP_X);

    // write zero page memory address to the value to load into accumulator
    cpu.writeTwoBytes(0x00001024, 0x4232);
    cpu.X = 0x0013;

    // write the values to be loaded to the accumulator on the zero page
    cpu.writeTwoBytes(0x00004245, 0x0000);

    cpu.start(6);

    // test accumulator is set to the correct high endian value
    EXPECT_EQ(cpu.A, 0x0000);

    // test zero and default flags are set
    EXPECT_EQ(cpu.P, 0b00100010);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 6);
}

TEST_F(LDATest, LoadAccumulator_ZeroPage_XIndexed_NFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_LDA_ZP_X);

    // write zero page memory address to the value to load into accumulator
    cpu.writeTwoBytes(0x00001024, 0x4232);
    cpu.X = 0x0013;

    // write the values to be loaded to the accumulator on the zero page
    cpu.writeTwoBytes(0x00004245, 0xFFEF);

    cpu.start(6);

    // test accumulator is set to the correct high endian value
    EXPECT_EQ(cpu.A, 0xFFEF);

    // test negative and default flags are set
    EXPECT_EQ(cpu.P, 0b10100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 6);
}
