#include <gtest/gtest.h>

#include <AlienCPUTest.h>

class STXTest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.Reset();
    }

    virtual void TearDown() {

    }
};


// STX Absolute TESTS
TEST_F(STXTest, SaveX_Absolute_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.WriteWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.WriteByte(0x00001023, AlienCPU::INS_STX_ABS);

    // write memory address to save the X to
    cpu.WriteWord(0x00001024, 0x00014232);
    cpu.X = 0x4232;

    cpu.Start(7);

    // test memory address stores X's value
    EXPECT_EQ(cpu.ReadTwoBytes(0x00014232), 0x4232);
    cpu.cycles -= 2; // nullify the extra cycles from the read

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001028);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 7);
}


// STX ZERO PAGE TESTS
TEST_F(STXTest, SaveX_ZeroPage_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.WriteWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.WriteByte(0x00001023, AlienCPU::INS_STX_ZP);

    // write zero page memory address to save the X
    cpu.WriteTwoBytes(0x00001024, 0x1232);
    cpu.X = 0x1234;

    cpu.Start(5);

    // test memory address stores X's value
    EXPECT_EQ(cpu.ReadTwoBytes(0x00001232), 0x1234);
    cpu.cycles -= 2; // nullify the extra cycles from the read

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 5);
}


// STX ZEROPAGE Y-INDEXED TESTS
TEST_F(STXTest, SaveX_ZeroPage_YIndexed_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.WriteWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.WriteByte(0x00001023, AlienCPU::INS_STX_ZP_Y);

    // write zero page memory address save the X
    cpu.WriteTwoBytes(0x00001024, 0x1232);
    cpu.Y = 0x0013;
    cpu.X = 0x1234;

    cpu.Start(6);

    // test memory address stores X's value
    EXPECT_EQ(cpu.ReadTwoBytes(0x00001245), 0x1234);
    cpu.cycles -= 2; // nullify the extra cycles from the read

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 6);
}

TEST_F(STXTest, SaveX_ZeroPage_YIndexed_WRAPAROUND) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.WriteWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.WriteByte(0x00001023, AlienCPU::INS_STX_ZP_Y);

    // write zero page memory address save the X
    cpu.WriteTwoBytes(0x00001024, 0x1232);
    cpu.Y = 0xF013;
    cpu.X = 0x1234;

    cpu.Start(6);

    // test memory address stores X's value
    EXPECT_EQ(cpu.ReadTwoBytes(0x00000245), 0x1234);
    cpu.cycles -= 2; // nullify the extra cycles from the read

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 6);
}