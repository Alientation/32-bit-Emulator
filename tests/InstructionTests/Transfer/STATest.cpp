#include <AlienCPUTest.h>

class STATest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};


// STA Absolute TESTS
TEST_F(STATest, SaveAccumulator_Absolute_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_STA_ABS);

    // write memory address to save the accumulator to
    cpu.writeWord(0x00001024, 0x00014232);
    cpu.A = 0x4232;

    cpu.start(7);

    // test memory address stores the accumulator's value
    EXPECT_EQ(cpu.readTwoBytes(0x00014232), 0x4232);
    cpu.cycles -= 2; // nullify the extra cycles from the read

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001028);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 7);
}

// STA Absolute XIndexed TESTS
TEST_F(STATest, SaveAccumulator_XIndexed_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_STA_ABS_X);

    // write memory address to save the accumulator to
    cpu.writeWord(0x00001024, 0x00014232);
    cpu.X = 0x0013;
    cpu.A = 0x4232;

    cpu.start(8);

    // test memory address stores the accumulator's value
    EXPECT_EQ(cpu.readTwoBytes(0x00014245), 0x4232);
    cpu.cycles -= 2; // nullify the extra cycles from the read

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001028);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 8);
}

// STA Absolute YIndexed TESTS
TEST_F(STATest, SaveAccumulator_YIndexed_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_STA_ABS_Y);

    // write memory address to save the accumulator to
    cpu.writeWord(0x00001024, 0x00014232);
    cpu.Y = 0x0013;
    cpu.A = 0x4232;

    cpu.start(8);

    // test memory address stores the accumulator's value
    EXPECT_EQ(cpu.readTwoBytes(0x00014245), 0x4232);
    cpu.cycles -= 2; // nullify the extra cycles from the read

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001028);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 8);
}


// STA X-INDEXED INDIRECT TESTS
TEST_F(STATest, StoreAccumulator_XIndexed_Indirect_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_STA_X_IND);

    // write zero page memory address of address to save the accumulator
    cpu.writeTwoBytes(0x00001024, 0x4232);
    cpu.X = 0x0013;
    cpu.A = 0x1234;

    // write memory address to save the accumulator
    cpu.writeWord(0x00004245, 0x00011234);

    cpu.start(10);

    // test memory address stores the accumulator's value
    EXPECT_EQ(cpu.readTwoBytes(0x00011234), 0x1234);
    cpu.cycles -= 2; // nullify the extra cycles from the read

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 10);
}

TEST_F(STATest, StoreAccumulator_XIndexed_Indirect_WRAPAROUND) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_STA_X_IND);

    // write zero page memory address of address to save the accumulator
    cpu.writeTwoBytes(0x00001024, 0x1232);
    cpu.X = 0xF013;
    cpu.A = 0x1234;

    // write memory address to save the accumulator
    cpu.writeWord(0x00000245, 0x00011234);

    cpu.start(10);

    // test memory address stores the accumulator's value
    EXPECT_EQ(cpu.readTwoBytes(0x00011234), 0x1234);
    cpu.cycles -= 2; // nullify the extra cycles from the read

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 10);
}


// STA INDIRECT YIndexed TESTS
TEST_F(STATest, StoreAccumulator_Indirect_YIndexed_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_STA_IND_Y);

    // write zero page memory address of address to save the accumulator
    cpu.writeTwoBytes(0x00001024, 0x4232);
    cpu.Y = 0x0013;
    cpu.A = 0x1234;

    // write memory address to save the accumulator
    cpu.writeWord(0x00004232, 0x00011234);

    cpu.start(10);

    // test memory address stores the accumulator's value
    EXPECT_EQ(cpu.readTwoBytes(0x00011247), 0x1234);
    cpu.cycles -= 2; // nullify the extra cycles from the read

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 10);
}

TEST_F(STATest, StoreAccumulator_Indirect_YIndexed_WRAPAROUND) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_STA_IND_Y);

    // write zero page memory address of address to save the accumulator
    cpu.writeTwoBytes(0x00001024, 0x1232);
    cpu.Y = 0xF013;
    cpu.A = 0x1234;

    // write memory address to save the accumulator
    cpu.writeWord(0x00001232, 0x00011234);

    cpu.start(10);

    // test memory address stores the accumulator's value
    EXPECT_EQ(cpu.readTwoBytes(0x00020247), 0x1234);
    cpu.cycles -= 2; // nullify the extra cycles from the read

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 10);
}


// STA ZERO PAGE TESTS
TEST_F(STATest, SaveAccumulator_ZeroPage_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_STA_ZP);

    // write zero page memory address to save the accumulator
    cpu.writeTwoBytes(0x00001024, 0x1232);
    cpu.A = 0x1234;

    cpu.start(5);

    // test memory address stores the accumulator's value
    EXPECT_EQ(cpu.readTwoBytes(0x00001232), 0x1234);
    cpu.cycles -= 2; // nullify the extra cycles from the read

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 5);
}


// STA ZEROPAGE X-INDEXED TESTS
TEST_F(STATest, SaveAccumulator_ZeroPage_XIndexed_NORMAL) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_STA_ZP_X);

    // write zero page memory address save the accumulator
    cpu.writeTwoBytes(0x00001024, 0x1232);
    cpu.X = 0x0013;
    cpu.A = 0x1234;

    cpu.start(6);

    // test memory address stores the accumulator's value
    EXPECT_EQ(cpu.readTwoBytes(0x00001245), 0x1234);
    cpu.cycles -= 2; // nullify the extra cycles from the read

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 6);
}

TEST_F(STATest, SaveAccumulator_ZeroPage_XIndexed_WRAPAROUND) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_STA_ZP_X);

    // write zero page memory address save the accumulator
    cpu.writeTwoBytes(0x00001024, 0x1232);
    cpu.X = 0xF013;
    cpu.A = 0x1234;

    cpu.start(6);

    // test memory address stores the accumulator's value
    EXPECT_EQ(cpu.readTwoBytes(0x00000245), 0x1234);
    cpu.cycles -= 2; // nullify the extra cycles from the read

    // test only default flag is set
    EXPECT_EQ(cpu.P, 0b00100000);

    // test PC
    EXPECT_EQ(cpu.PC, 0x00001026);

    // test cycle counter
    EXPECT_EQ(cpu.cycles, 6);
}