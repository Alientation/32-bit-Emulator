#include <AlienCPUTest.h>

class ASLTest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};


// ASL ACCUMULATOR TESTS
TEST_F(ASLTest, ArithmeticShiftLeft_Accumulator_NORMAL) {

}


// ASL ABSOLUTE TESTS
TEST_F(ASLTest, ArithmeticShiftLeft_Absolute_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_ASL_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00012345); // address of value to shift left
    cpu.writeByte(0x00012345, 0x12); // value to shift left
}