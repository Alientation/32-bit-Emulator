#include <emulator6502_tests\AlienCPUTest.h>

class STXTest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};


// STX ABSOLUTE TESTS
TEST_F(STXTest, StoreX_Absolute_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_STX_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00014232); // address to store X
    cpu.X = 0x4232;

    TestInstruction(cpu, 7, 0x00001028);

    // test memory address stores X's value
    EXPECT_EQ(cpu.motherboard.ram[0x00014232], 0x32) << "Low byte of X not stored in memory";
    EXPECT_EQ(cpu.motherboard.ram[0x00014233], 0x42) << "High byte of X not stored in memory";
    EXPECT_EQ(cpu.X, 0x4232) << "X should not be altered";
    TestUnchangedState(cpu, A, Y, SP, P);
}


// STX ZEROPAGE TESTS
TEST_F(STXTest, StoreX_ZeroPage_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_STX_ZP, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1232); // zp address to store X
    cpu.X = 0x1234;

    TestInstruction(cpu, 5, 0x00001026);

    // test memory address stores X's value
    EXPECT_EQ(cpu.motherboard.ram[0x00001232], 0x34) << "Low byte of X not stored in memory";
    EXPECT_EQ(cpu.motherboard.ram[0x00001233], 0x12) << "High byte of X not stored in memory";
    EXPECT_EQ(cpu.X, 0x1234) << "X should not be altered";
    TestUnchangedState(cpu, A, Y, SP, P);
}


// STX ZEROPAGE Y-INDEXED TESTS
TEST_F(STXTest, StoreX_ZeroPage_YIndexed_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_STX_ZP_Y, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1232); // partial zp address save X
    cpu.Y = 0x0013;
    cpu.X = 0x1234;

    TestInstruction(cpu, 6, 0x00001026);

    // test memory address stores X's value
    EXPECT_EQ(cpu.motherboard.ram[0x00001245], 0x34) << "Low byte of X not stored in memory";
    EXPECT_EQ(cpu.motherboard.ram[0x00001246], 0x12) << "High byte of X not stored in memory";
    EXPECT_EQ(cpu.X, 0x1234) << "X should not be altered";
    EXPECT_EQ(cpu.Y, 0x0013) << "Y should not be altered";
    TestUnchangedState(cpu, A, SP, P);
}

TEST_F(STXTest, StoreX_ZeroPage_YIndexed_WRAPAROUND) {
    LoadInstruction(cpu, AlienCPU::INS_STX_ZP_Y, 0x00001023);
    cpu.writeTwoBytes(0x00001024, 0x1232); // partial zp address save X
    cpu.Y = 0xF013;
    cpu.X = 0x1234;

    TestInstruction(cpu, 6, 0x00001026);

    // test memory address stores X's value
    EXPECT_EQ(cpu.motherboard.ram[0x00000245], 0x34) << "Low byte of X not stored in memory";
    EXPECT_EQ(cpu.motherboard.ram[0x00000246], 0x12) << "High byte of X not stored in memory";
    EXPECT_EQ(cpu.X, 0x1234) << "X should not be altered";
    EXPECT_EQ(cpu.Y, 0xF013) << "Y should not be altered";
    TestUnchangedState(cpu, A, SP, P);
}