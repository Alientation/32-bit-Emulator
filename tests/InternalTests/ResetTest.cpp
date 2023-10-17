#include <gtest/gtest.h>
#include <AlienCPUTest.h>

class ResetTest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};


TEST_F(ResetTest, ResetTest_Default) {
    cpu.PC = 0x00012345;
    cpu.SP = 0x0000FFFB;
    cpu.motherboard.ram.data[0x00000000] = 0x78;
    cpu.motherboard.ram.data[0x00000001] = 0x56;
    cpu.motherboard.ram.data[0x00000002] = 0x34;
    cpu.motherboard.ram.data[0x00000003] = 0x12;
    cpu.A = 0x1234;
    cpu.X = 0x5678;
    cpu.Y = 0x9ABC;
    cpu.P = 0b11111111;
    cpu.cycles = 3434;
    cpu.motherboard.rom.data[0x00000000] = 0x12;
    cpu.motherboard.rom.data[0x00000001] = 0x34;
    cpu.motherboard.rom.data[0x00000002] = 0x56;
    cpu.motherboard.rom.data[0x00000003] = 0x78;

    cpu.reset();

    ASSERT_EQ(cpu.PC, cpu.PC_INIT);
    ASSERT_EQ(cpu.SP, cpu.SP_INIT);
    //ASSERT_EQ(cpu.motherboard.ram.data[0x00000000], 0x00);
    //ASSERT_EQ(cpu.motherboard.ram.data[0x00000001], 0x00);
    //ASSERT_EQ(cpu.motherboard.ram.data[0x00000002], 0x00);
    //ASSERT_EQ(cpu.motherboard.ram.data[0x00000003], 0x00);
    ASSERT_EQ(cpu.A, cpu.A_INIT);
    ASSERT_EQ(cpu.X, cpu.X_INIT);
    ASSERT_EQ(cpu.Y, cpu.Y_INIT);
    ASSERT_EQ(cpu.P, cpu.P_INIT);
    ASSERT_EQ(cpu.cycles, 0);
    ASSERT_EQ(cpu.motherboard.rom.data[0x00000000], 0x12);
    ASSERT_EQ(cpu.motherboard.rom.data[0x00000001], 0x34);
    ASSERT_EQ(cpu.motherboard.rom.data[0x00000002], 0x56);
    ASSERT_EQ(cpu.motherboard.rom.data[0x00000003], 0x78);
}