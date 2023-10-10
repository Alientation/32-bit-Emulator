#include <gtest/gtest.h>

#include <AlienCPUTest.h>

class STYTest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};


// STY Absolute TESTS
TEST_F(STYTest, SaveY_Absolute_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_STY_ABS);

    // write memory address to save the Y to
    cpu.writeWord(0x00001024, 0x00014232);
    cpu.Y = 0x4232;

    cpu.start(7);

    // test memory address stores Y's value
    EXPECT_EQ(cpu.readTwoBytes(0x00014232), 0x4232);
    cpu.cycles -= 2; // nullify the extra cycles from the read

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001028);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 7);
}


// STY ZERO PAGE TESTS
TEST_F(STYTest, SaveY_ZeroPage_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_STY_ZP);

    // write zero page memory address to save the Y
    cpu.writeTwoBytes(0x00001024, 0x1232);
    cpu.Y = 0x1234;

    cpu.start(5);

    // test memory address stores Y's value
    EXPECT_EQ(cpu.readTwoBytes(0x00001232), 0x1234);
    cpu.cycles -= 2; // nullify the extra cycles from the read

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 5);
}


// STY ZEROPAGE Y-INDEXED TESTS
TEST_F(STYTest, SaveY_ZeroPage_YIndexed_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_STY_ZP_X);

    // write zero page memory address save the Y
    cpu.writeTwoBytes(0x00001024, 0x1232);
    cpu.X = 0x0013;
    cpu.Y = 0x1234;

    cpu.start(6);

    // test memory address stores Y's value
    EXPECT_EQ(cpu.readTwoBytes(0x00001245), 0x1234);
    cpu.cycles -= 2; // nullify the extra cycles from the read

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 6);
}

TEST_F(STYTest, SaveY_ZeroPage_YIndexed_WRAPAROUND) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_STY_ZP_X);

    // write zero page memory address save the Y
    cpu.writeTwoBytes(0x00001024, 0x1232);
    cpu.X = 0xF013;
    cpu.Y = 0x1234;

    cpu.start(6);

    // test memory address stores Y's value
    EXPECT_EQ(cpu.readTwoBytes(0x00000245), 0x1234);
    cpu.cycles -= 2; // nullify the extra cycles from the read

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 6);
}