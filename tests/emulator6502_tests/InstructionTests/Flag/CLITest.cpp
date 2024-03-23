#include <emulator6502_tests\AlienCPUTest.h>

class CLITest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};