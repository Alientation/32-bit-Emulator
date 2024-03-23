#include <emulator6502_tests\AlienCPUTest.h>

class STATest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};


// STA ABSOLUTE TESTS
TEST_F(STATest, StoreAccumulator_Absolute_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_STA_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00014232); // address to store the accumulator
    cpu.A = 0x4232;

    TestInstruction(cpu, 7, 0x00001028);

    // test memory address stores the accumulator's value
    EXPECT_EQ(cpu.motherboard.ram[0x00014232], 0x32) << "Low byte of accumulator not stored in memory";
    EXPECT_EQ(cpu.motherboard.ram[0x00014233], 0x42) << "High byte of accumulator not stored in memory";
    EXPECT_EQ(cpu.A, 0x4232) << "Accumulator should not be altered";
    TestUnchangedState(cpu, X, Y, SP, P);
}


// STA ABSOLUTE XIndexed TESTS
TEST_F(STATest, StoreAccumulator_Absolute_XIndexed_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_STA_ABS_X, 0x00001023);
    cpu.writeWord(0x00001024, 0x00014232); // partial address to store the accumulator
    cpu.X = 0x0013;
    cpu.A = 0x4232;

    TestInstruction(cpu, 8, 0x00001028);

    // test memory address stores the accumulator's value
    EXPECT_EQ(cpu.motherboard.ram[0x00014245], 0x32) << "Low byte of accumulator not stored in memory";
    EXPECT_EQ(cpu.motherboard.ram[0x00014246], 0x42) << "High byte of accumulator not stored in memory";
    EXPECT_EQ(cpu.A, 0x4232) << "Accumulator should not be altered";
    EXPECT_EQ(cpu.X, 0x0013) << "X register should not be altered";
    TestUnchangedState(cpu, Y, SP, P);
}

TEST_F(STATest, StoreAccumulator_Absolute_XIndexed_PAGECROSS) {
    LoadInstruction(cpu, AlienCPU::INS_STA_ABS_X, 0x00001023);
    cpu.writeWord(0x00001024, 0x00011232); // partial address to store the accumulator
    cpu.X = 0xF013;
    cpu.A = 0x4232;

    TestInstruction(cpu, 8, 0x00001028);

    // test memory address stores the accumulator's value
    EXPECT_EQ(cpu.motherboard.ram[0x00020245], 0x32) << "Low byte of accumulator not stored in memory";
    EXPECT_EQ(cpu.motherboard.ram[0x00020246], 0x42) << "High byte of accumulator not stored in memory";
    EXPECT_EQ(cpu.A, 0x4232) << "Accumulator should not be altered";
    EXPECT_EQ(cpu.X, 0xF013) << "X register should not be altered";
    TestUnchangedState(cpu, Y, SP, P);
}


// STA ABSOLUTE YIndexed TESTS
TEST_F(STATest, StoreAccumulator_Absolute_YIndexed_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_STA_ABS_Y, 0x00001023);
    cpu.writeWord(0x00001024, 0x00014232); // partial address to store the accumulator
    cpu.Y = 0x0013;
    cpu.A = 0x4232;

    TestInstruction(cpu, 8, 0x00001028);

    // test memory address stores the accumulator's value
    EXPECT_EQ(cpu.motherboard.ram[0x00014245], 0x32) << "Low byte of accumulator not stored in memory";
    EXPECT_EQ(cpu.motherboard.ram[0x00014246], 0x42) << "High byte of accumulator not stored in memory";
    EXPECT_EQ(cpu.A, 0x4232) << "Accumulator should not be altered";
    EXPECT_EQ(cpu.Y, 0x0013) << "Y register should not be altered";
    TestUnchangedState(cpu, X, SP, P);
}

TEST_F(STATest, StoreAccumulator_Absolute_YIndexed_PAGECROSS) {
    LoadInstruction(cpu, AlienCPU::INS_STA_ABS_Y, 0x00001023);
    cpu.writeWord(0x00001024, 0x00011232); // partial address to store the accumulator
    cpu.Y = 0xF013;
    cpu.A = 0x4232;

    TestInstruction(cpu, 8, 0x00001028);

    // test memory address stores the accumulator's value
    EXPECT_EQ(cpu.motherboard.ram[0x00020245], 0x32) << "Low byte of accumulator not stored in memory";
    EXPECT_EQ(cpu.motherboard.ram[0x00020246], 0x42) << "High byte of accumulator not stored in memory";
    EXPECT_EQ(cpu.A, 0x4232) << "Accumulator should not be altered";
    EXPECT_EQ(cpu.Y, 0xF013) << "Y register should not be altered";
    TestUnchangedState(cpu, X, SP, P);
}


// STA ZEROPAGE TESTS
TEST_F(STATest, StoreAccumulator_ZeroPage_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_STA_ZP, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1232); // zp address to store the accumulator
    cpu.A = 0x1234;

    TestInstruction(cpu, 5, 0x00001026);

    // test memory address stores the accumulator's value
    EXPECT_EQ(cpu.motherboard.ram[0x00001232], 0x34) << "Low byte of accumulator not stored in memory";
    EXPECT_EQ(cpu.motherboard.ram[0x00001233], 0x12) << "High byte of accumulator not stored in memory";
    EXPECT_EQ(cpu.A, 0x1234) << "Accumulator should not be altered";
    TestUnchangedState(cpu, X, Y, SP, P);
}


// STA ZEROPAGE X-INDEXED TESTS
TEST_F(STATest, StoreAccumulator_ZeroPage_XIndexed_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_STA_ZP_X, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1232); // partial zp address store the accumulator
    cpu.X = 0x0013;
    cpu.A = 0x1234;

    TestInstruction(cpu, 6, 0x00001026);

    // test memory address stores the accumulator's value
    EXPECT_EQ(cpu.motherboard.ram[0x00001245], 0x34) << "Low byte of accumulator not stored in memory";
    EXPECT_EQ(cpu.motherboard.ram[0x00001246], 0x12) << "High byte of accumulator not stored in memory";
    EXPECT_EQ(cpu.A, 0x1234) << "Accumulator should not be altered";
    EXPECT_EQ(cpu.X, 0x0013) << "X register should not be altered";
    TestUnchangedState(cpu, Y, SP, P);
}

TEST_F(STATest, StoreAccumulator_ZeroPage_XIndexed_WRAPAROUND) {
    LoadInstruction(cpu, AlienCPU::INS_STA_ZP_X, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1232); // partial zp address store the accumulator
    cpu.X = 0xF013;
    cpu.A = 0x1234;

    TestInstruction(cpu, 6, 0x00001026);

    // test memory address stores the accumulator's value
    EXPECT_EQ(cpu.motherboard.ram[0x00000245], 0x34) << "Low byte of accumulator not stored in memory";
    EXPECT_EQ(cpu.motherboard.ram[0x00000246], 0x12) << "High byte of accumulator not stored in memory";
    EXPECT_EQ(cpu.A, 0x1234) << "Accumulator should not be altered";
    EXPECT_EQ(cpu.X, 0xF013) << "X register should not be altered";
    TestUnchangedState(cpu, Y, SP, P);
}


// STA X-INDEXED INDIRECT TESTS
TEST_F(STATest, StoreAccumulator_XIndexed_Indirect_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_STA_X_IND, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x4232); // partial zp address of address to store the accumulator
    cpu.X = 0x0013;
    cpu.writeWord(0x00004245, 0x00011234); // address to store the accumulator
    cpu.A = 0x1234;

    TestInstruction(cpu, 10, 0x00001026);

    // test memory address stores the accumulator's value
    EXPECT_EQ(cpu.motherboard.ram[0x00011234], 0x34) << "Low byte of accumulator not stored in memory";
    EXPECT_EQ(cpu.motherboard.ram[0x00011235], 0x12) << "High byte of accumulator not stored in memory";
    EXPECT_EQ(cpu.A, 0x1234) << "Accumulator should not be altered";
    EXPECT_EQ(cpu.X, 0x0013) << "X register should not be altered";
    TestUnchangedState(cpu, Y, SP, P);
}

TEST_F(STATest, StoreAccumulator_XIndexed_Indirect_WRAPAROUND) {
    LoadInstruction(cpu, AlienCPU::INS_STA_X_IND, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1232); // partial zp address of address to store the accumulator
    cpu.X = 0xF013;
    cpu.writeWord(0x00000245, 0x00011234); // address to store the accumulator
    cpu.A = 0x1234;

    TestInstruction(cpu, 10, 0x00001026);

    // test memory address stores the accumulator's value
    EXPECT_EQ(cpu.motherboard.ram[0x00011234], 0x34) << "Low byte of accumulator not stored in memory";
    EXPECT_EQ(cpu.motherboard.ram[0x00011235], 0x12) << "High byte of accumulator not stored in memory";
    EXPECT_EQ(cpu.A, 0x1234) << "Accumulator should not be altered";
    EXPECT_EQ(cpu.X, 0xF013) << "X register should not be altered";
    TestUnchangedState(cpu, Y, SP, P);
}


// STA INDIRECT YIndexed TESTS
TEST_F(STATest, StoreAccumulator_Indirect_YIndexed_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_STA_IND_Y, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x4232); // zp address of partial address to store the accumulator
    cpu.writeWord(0x00004232, 0x00011234); // partial address to store the accumulator
    cpu.Y = 0x0013;
    cpu.A = 0x1234;

    TestInstruction(cpu, 10, 0x00001026);

    // test memory address stores the accumulator's value
    EXPECT_EQ(cpu.motherboard.ram[0x00011247], 0x34) << "Low byte of accumulator not stored in memory";
    EXPECT_EQ(cpu.motherboard.ram[0x00011248], 0x12) << "High byte of accumulator not stored in memory";
    EXPECT_EQ(cpu.A, 0x1234) << "Accumulator should not be altered";
    EXPECT_EQ(cpu.Y, 0x0013) << "Y register should not be altered";
    TestUnchangedState(cpu, X, SP, P);
}

TEST_F(STATest, StoreAccumulator_Indirect_YIndexed_PAGECROSS) {
    LoadInstruction(cpu, AlienCPU::INS_STA_IND_Y, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1232); // zp address of partial address to store the accumulator
    cpu.writeWord(0x00001232, 0x00011234); // partial address to store the accumulator
    cpu.Y = 0xF013;
    cpu.A = 0x1234;

    TestInstruction(cpu, 10, 0x00001026);

    // test memory address stores the accumulator's value
    EXPECT_EQ(cpu.motherboard.ram[0x00020247], 0x34) << "Low byte of accumulator not stored in memory";
    EXPECT_EQ(cpu.motherboard.ram[0x00020248], 0x12) << "High byte of accumulator not stored in memory";
    EXPECT_EQ(cpu.A, 0x1234) << "Accumulator should not be altered";
    EXPECT_EQ(cpu.Y, 0xF013) << "Y register should not be altered";
    TestUnchangedState(cpu, X, SP, P);
}