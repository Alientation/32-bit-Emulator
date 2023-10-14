#include <AlienCPUTest.h>

class INXTest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};


TEST_F(INXTest, IncrementXImplied_Normal) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_INX_IMPL);
    cpu.X = 0x1234;

    cpu.start(2);

    EXPECT_EQ(cpu.X, 0x1235); // check incremented memory value
    EXPECT_EQ(cpu.PC, 0x00001024); // check PC points to next instruction
    EXPECT_EQ(cpu.cycles, 2);
    EXPECT_EQ(cpu.P, 0b00100000); // only default flag is set
}

TEST_F(INXTest, IncrementXImplied_ZEROFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_INX_IMPL);
    cpu.X = 0xFFFF;

    cpu.start(2);

    EXPECT_EQ(cpu.X, 0x0000); // check incremented memory value
    EXPECT_EQ(cpu.PC, 0x00001024); // check PC points to next instruction
    EXPECT_EQ(cpu.cycles, 2);
    EXPECT_EQ(cpu.P, 0b00100010); // only default flag is set
}

TEST_F(INXTest, IncrementXImplied_NEGATIVEFLAG) {
    // setting reset vector to begin processing instructions at 0x0001023
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_INX_IMPL);
    cpu.X = 0xFFFE;

    cpu.start(2);

    EXPECT_EQ(cpu.X, 0xFFFF); // check incremented memory value
    EXPECT_EQ(cpu.PC, 0x00001024); // check PC points to next instruction
    EXPECT_EQ(cpu.cycles, 2);
    EXPECT_EQ(cpu.P, 0b10100000); // only default flag is set
}