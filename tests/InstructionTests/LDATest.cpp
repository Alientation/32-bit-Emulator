#include <gtest/gtest.h>

#include <AlienCPUTest.h>

class LDATest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.Reset();
    }

    virtual void TearDown() {

    }
};


// LDA IMMEDIATE TESTS
TEST_F(LDATest, LoadAccumulator_Immediate_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.WriteWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    
    // test instruction load accumulator immediate addressing
    cpu.WriteByte(0x00001023, AlienCPU::INS_LDA_IM);

    // load in value 0x4232 (stored in little endian as $32 $42)
    cpu.WriteTwoBytes(0x00001024, 0x4232);

    cpu.Start(3);

    // test accumulator is set to the correct high endian value
    EXPECT_EQ(cpu.A, 0x4232);

    // test flags
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 3);
}

TEST_F(LDATest, LoadAccumulator_Immediate_ZFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.WriteWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    
    // test instruction load accumulator immediate addressing
    cpu.WriteByte(0x00001023, AlienCPU::INS_LDA_IM);

    // load in value 0x0000 (stored in little endian as $00 $00)
    cpu.WriteTwoBytes(0x00001024, 0x0000);

    cpu.Start(3);

    // test accumulator is set to the correct high endian value
    EXPECT_EQ(cpu.A, 0x0000);

    // test flags
    EXPECT_EQ(cpu.P, 0b00100010);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 3);
}

TEST_F(LDATest, LoadAccumulator_Immediate_NFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.WriteWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    
    // test instruction load accumulator immediate addressing
    cpu.WriteByte(0x00001023, AlienCPU::INS_LDA_IM);

    // load in value 0xFFEF (stored in little endian as $EF $FF)
    cpu.WriteTwoBytes(0x00001024, 0xFFEF);

    cpu.Start(3);

    // test accumulator is set to the correct high endian value
    EXPECT_EQ(cpu.A, 0xFFEF);

    // test flags
    EXPECT_EQ(cpu.P, 0b10100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 3);
}


// LDA ZERO PAGE TESTS
TEST_F(LDATest, LoadAccumulator_ZeroPage_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.WriteWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    
    // test instruction load accumulator immediate addressing
    cpu.WriteByte(0x00001023, AlienCPU::INS_LDA_ZP);

    // load in value 0x4232 (stored in little endian as $32 $42)
    cpu.WriteTwoBytes(0x00001024, 0x4232);

    // write the values to be loaded to the accumulator on the zero page
    cpu.WriteTwoBytes(0x00004232, 0x2042);

    cpu.Start(5);

    // test accumulator is set to the correct high endian value
    EXPECT_EQ(cpu.A, 0x2042);

    // test flags
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 5);
}

TEST_F(LDATest, LoadAccumulator_ZeroPage_ZFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.WriteWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    
    // test instruction load accumulator immediate addressing
    cpu.WriteByte(0x00001023, AlienCPU::INS_LDA_ZP);

    // load in value 0x4232 (stored in little endian as $32 $42)
    cpu.WriteTwoBytes(0x00001024, 0x4232);

    // write the values to be loaded to the accumulator on the zero page
    cpu.WriteTwoBytes(0x00004232, 0x0000);

    cpu.Start(5);

    // test accumulator is set to the correct high endian value
    EXPECT_EQ(cpu.A, 0x0000);

    // test flags
    EXPECT_EQ(cpu.P, 0b00100010);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 5);
}

TEST_F(LDATest, LoadAccumulator_ZeroPage_NFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.WriteWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    
    // test instruction load accumulator immediate addressing
    cpu.WriteByte(0x00001023, AlienCPU::INS_LDA_ZP);

    // load in value 0x4232 (stored in little endian as $32 $42)
    cpu.WriteTwoBytes(0x00001024, 0x4232);

    // write the values to be loaded to the accumulator on the zero page
    cpu.WriteTwoBytes(0x00004232, 0xFFEF);

    cpu.Start(5);

    // test accumulator is set to the correct high endian value
    EXPECT_EQ(cpu.A, 0xFFEF);

    // test flags
    EXPECT_EQ(cpu.P, 0b10100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 5);
}