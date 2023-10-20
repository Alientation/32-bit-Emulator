#include <gtest/gtest.h>
#include <AlienCPUTest.h>

class MemoryAddressingTest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};

TEST_F(MemoryAddressingTest, MemoryAddressingTest_ConvertToLowEndian) {
    EXPECT_EQ(cpu.convertToLowEndianWord(0x12345678), 0x78563412);
    EXPECT_EQ(cpu.convertToLowEndianWord(0x00000000), 0x00000000);

    EXPECT_EQ(cpu.convertToLowEndianTwoBytes(0x1234), 0x3412);
    EXPECT_EQ(cpu.convertToLowEndianTwoBytes(0x0000), 0x0000);
}

TEST_F(MemoryAddressingTest, MemoryAddressingTest_ConvertToHighEndian) {
    EXPECT_EQ(cpu.convertToHighEndianWord(0x78563412), 0x12345678);
    EXPECT_EQ(cpu.convertToHighEndianWord(0x00000000), 0x00000000);

    EXPECT_EQ(cpu.convertToHighEndianTwoBytes(0x3412), 0x1234);
    EXPECT_EQ(cpu.convertToHighEndianTwoBytes(0x0000), 0x0000);
}

TEST_F(MemoryAddressingTest, MemoryAddressingTest_Read) {
    cpu.motherboard.ram.data[0x0000] = 0x78;
    cpu.motherboard.ram.data[0x0001] = 0x56;
    cpu.motherboard.ram.data[0x0002] = 0x34;
    cpu.motherboard.ram.data[0x0003] = 0x12;

    EXPECT_EQ(cpu.readWord(0x00000000), 0x12345678);
    EXPECT_EQ(cpu.readWord(0x00000001), 0x00123456);
    EXPECT_EQ(cpu.readWord(0x00000002), 0x00001234);
    EXPECT_EQ(cpu.readWord(0x00000003), 0x00000012);
    EXPECT_EQ(cpu.readWord(0x00000004), 0x00000000);

    EXPECT_EQ(cpu.readTwoBytes(0x00000000), 0x5678);
    EXPECT_EQ(cpu.readTwoBytes(0x00000001), 0x3456);
    EXPECT_EQ(cpu.readTwoBytes(0x00000002), 0x1234);
    EXPECT_EQ(cpu.readTwoBytes(0x00000003), 0x0012);
    EXPECT_EQ(cpu.readTwoBytes(0x00000004), 0x0000);

    EXPECT_EQ(cpu.readByte(0x00000000), 0x78);
    EXPECT_EQ(cpu.readByte(0x00000001), 0x56);
    EXPECT_EQ(cpu.readByte(0x00000002), 0x34);
    EXPECT_EQ(cpu.readByte(0x00000003), 0x12);
    EXPECT_EQ(cpu.readByte(0x00000004), 0x00);
}

TEST_F(MemoryAddressingTest, MemoryAddressingTest_Write) {
    cpu.writeByte(0x00000000, 0x12);
    cpu.writeByte(0x00000001, 0x34);
    cpu.writeByte(0x00000002, 0x56);
    cpu.writeByte(0x00000003, 0x78);

    EXPECT_EQ(cpu.cycles, 4);
    EXPECT_EQ(cpu.motherboard.ram.data[0x0000], 0x12);
    EXPECT_EQ(cpu.motherboard.ram.data[0x0001], 0x34);
    EXPECT_EQ(cpu.motherboard.ram.data[0x0002], 0x56);
    EXPECT_EQ(cpu.motherboard.ram.data[0x0003], 0x78);
    EXPECT_EQ(cpu.motherboard.ram.data[0x0004], 0x00);

    cpu.writeTwoBytes(0x00001000, 0x1234);
    cpu.writeTwoBytes(0x00001002, 0x5678);

    EXPECT_EQ(cpu.cycles, 8);
    EXPECT_EQ(cpu.motherboard.ram.data[0x1000], 0x34);
    EXPECT_EQ(cpu.motherboard.ram.data[0x1001], 0x12);
    EXPECT_EQ(cpu.motherboard.ram.data[0x1002], 0x78);
    EXPECT_EQ(cpu.motherboard.ram.data[0x1003], 0x56);
    EXPECT_EQ(cpu.motherboard.ram.data[0x1004], 0x00);

    cpu.writeWord(0x00002000, 0x12345678);

    EXPECT_EQ(cpu.cycles, 12);
    EXPECT_EQ(cpu.motherboard.ram.data[0x2000], 0x78);
    EXPECT_EQ(cpu.motherboard.ram.data[0x2001], 0x56);
    EXPECT_EQ(cpu.motherboard.ram.data[0x2002], 0x34);
    EXPECT_EQ(cpu.motherboard.ram.data[0x2003], 0x12);
    EXPECT_EQ(cpu.motherboard.ram.data[0x2004], 0x00);
}

TEST_F(MemoryAddressingTest, MemoryAddressingTest_Fetch) {
    cpu.motherboard.ram.data[0x0000] = 0x78;
    cpu.motherboard.ram.data[0x0001] = 0x56;
    cpu.motherboard.ram.data[0x0002] = 0x34;
    cpu.motherboard.ram.data[0x0003] = 0x12;

    cpu.PC = 0x00000000;
    EXPECT_EQ(cpu.fetchNextByte(), 0x78);
    EXPECT_EQ(cpu.PC, 0x00000001);
    EXPECT_EQ(cpu.cycles, 1);
    EXPECT_EQ(cpu.fetchNextByte(), 0x56);
    EXPECT_EQ(cpu.PC, 0x00000002);
    EXPECT_EQ(cpu.cycles, 2);
    EXPECT_EQ(cpu.fetchNextByte(), 0x34);
    EXPECT_EQ(cpu.PC, 0x00000003);
    EXPECT_EQ(cpu.cycles, 3);
    EXPECT_EQ(cpu.fetchNextByte(), 0x12);
    EXPECT_EQ(cpu.PC, 0x00000004);
    EXPECT_EQ(cpu.cycles, 4);
    EXPECT_EQ(cpu.fetchNextByte(), 0x00);
    EXPECT_EQ(cpu.PC, 0x00000005);
    EXPECT_EQ(cpu.cycles, 5);

    cpu.motherboard.ram.data[0x0000] = 0x78;
    cpu.motherboard.ram.data[0x0001] = 0x56;
    cpu.motherboard.ram.data[0x0002] = 0x34;
    cpu.motherboard.ram.data[0x0003] = 0x12;

    cpu.PC = 0x00000000;
    cpu.cycles = 0;
    EXPECT_EQ(cpu.fetchNextTwoBytes(), 0x5678);
    EXPECT_EQ(cpu.PC, 0x00000002);
    EXPECT_EQ(cpu.cycles, 2);
    EXPECT_EQ(cpu.fetchNextTwoBytes(), 0x1234);
    EXPECT_EQ(cpu.PC, 0x00000004);
    EXPECT_EQ(cpu.cycles, 4);
    EXPECT_EQ(cpu.fetchNextTwoBytes(), 0x0000);
    EXPECT_EQ(cpu.PC, 0x00000006);
    EXPECT_EQ(cpu.cycles, 6);

    cpu.motherboard.ram.data[0x0000] = 0x78;
    cpu.motherboard.ram.data[0x0001] = 0x56;
    cpu.motherboard.ram.data[0x0002] = 0x34;
    cpu.motherboard.ram.data[0x0003] = 0x12;

    cpu.PC = 0x00000000;
    cpu.cycles = 0;
    EXPECT_EQ(cpu.fetchNextWord(), 0x12345678);
    EXPECT_EQ(cpu.PC, 0x00000004);
    EXPECT_EQ(cpu.cycles, 4);
    EXPECT_EQ(cpu.fetchNextWord(), 0x00000000);
    EXPECT_EQ(cpu.PC, 0x00000008);
    EXPECT_EQ(cpu.cycles, 8);
}