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
TEST_F(STYTest, StoreY_Absolute_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_STY_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00014232); // address to store Y
    cpu.Y = 0x4232;

    TestInstruction(cpu, 7, 0x00001028);

    // test memory address stores Y's value
    EXPECT_EQ(cpu.motherboard.ram[0x00014232], 0x32) << "Low byte of Y not stored in memory";
    EXPECT_EQ(cpu.motherboard.ram[0x00014233], 0x42) << "High byte of Y not stored in memory";
    EXPECT_EQ(cpu.Y, 0x4232) << "Y should not be altered";
    TestUnchangedState(cpu, A, X, SP, P);
}


// STY ZEROPAGE TESTS
TEST_F(STYTest, StoreY_ZeroPage_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_STY_ZP, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1232); // zp address to store Y
    cpu.Y = 0x1234;

    TestInstruction(cpu, 5, 0x00001026);

    // test memory address stores Y's value
    EXPECT_EQ(cpu.motherboard.ram[0x00001232], 0x34) << "Low byte of Y not stored in memory";
    EXPECT_EQ(cpu.motherboard.ram[0x00001233], 0x12) << "High byte of Y not stored in memory";
    EXPECT_EQ(cpu.Y, 0x1234) << "Y should not be altered";
    TestUnchangedState(cpu, A, X, SP, P);
}


// STY ZEROPAGE Y-INDEXED TESTS
TEST_F(STYTest, StoreY_ZeroPage_YIndexed_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_STY_ZP_X, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1232); // partial zp address store Y
    cpu.X = 0x0013;
    cpu.Y = 0x1234;

    TestInstruction(cpu, 6, 0x00001026);

    // test memory address stores Y's value
    EXPECT_EQ(cpu.motherboard.ram[0x00001245], 0x34) << "Low byte of Y not stored in memory";
    EXPECT_EQ(cpu.motherboard.ram[0x00001246], 0x12) << "High byte of Y not stored in memory";
    EXPECT_EQ(cpu.Y, 0x1234) << "Y should not be altered";
    EXPECT_EQ(cpu.X, 0x0013) << "X should not be altered";
    TestUnchangedState(cpu, A, SP, P);
}

TEST_F(STYTest, StoreY_ZeroPage_YIndexed_WRAPAROUND) {
    LoadInstruction(cpu, AlienCPU::INS_STY_ZP_X, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1232); // partial zp address store Y
    cpu.X = 0xF013;
    cpu.Y = 0x1234;

    TestInstruction(cpu, 6, 0x00001026);

    // test memory address stores Y's value
    EXPECT_EQ(cpu.motherboard.ram[0x00000245], 0x34) << "Low byte of Y not stored in memory";
    EXPECT_EQ(cpu.motherboard.ram[0x00000246], 0x12) << "High byte of Y not stored in memory";
    EXPECT_EQ(cpu.Y, 0x1234) << "Y should not be altered";
    EXPECT_EQ(cpu.X, 0xF013) << "X should not be altered";
    TestUnchangedState(cpu, A, SP, P);
}