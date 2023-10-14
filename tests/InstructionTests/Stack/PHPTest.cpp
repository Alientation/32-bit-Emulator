#include <AlienCPUTest.h>

class PHPTest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};

TEST_F(PHPTest, PushProcessorImplied_Normal) {
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, 0x00001023);
    cpu.writeByte(0x00001023, AlienCPU::INS_PHP_IMPL);

    cpu.P = 0b01010101;

    cpu.start(2);

    EXPECT_EQ(cpu.motherboard.ram[0x0001FFFF], 0b01010101);
    EXPECT_EQ(cpu.SP, 0xFFFE);
    EXPECT_EQ(cpu.cycles, 2);
    EXPECT_EQ(cpu.PC, 0x00001024);
}