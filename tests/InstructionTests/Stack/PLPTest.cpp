#include <AlienCPUTest.h>

class PLPTest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};

TEST_F(PLPTest, PopProcessorImplied_Normal) {
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_PLP_IMPL);

    cpu.motherboard.ram[0x0001FFFF] = 0b01010101;
    cpu.SP = 0xFFFE;

    cpu.start(2);

    EXPECT_EQ(cpu.P, 0b01010101);
    EXPECT_EQ(cpu.SP, 0xFFFF);
    EXPECT_EQ(cpu.cycles, 2);
    EXPECT_EQ(cpu.PC, 0x00001024);
}