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


TEST_F(LDATest, LoadAccumulator_Immediate) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.WriteWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    
    // test instruction load accumulator immediate addressing
    cpu.WriteByte(0x00001023, AlienCPU::INS_LDA_IM);

    // load in value 0x4232 (stored in little endian as $32 $42)
    cpu.WriteTwoBytes(0x00001024, 0x4232);

    cpu.Start(3);

    // test accumulator is set to the correct high endian value
    EXPECT_EQ(cpu.A, 0x4232);

    // test flags 010100010
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 3);
}

