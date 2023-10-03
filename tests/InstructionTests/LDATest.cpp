#include <gtest/gtest.h>

#include <AlienCPUTest.h>

class LDATest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.Reset();
    }

    virtual void TearDown() {

    }



};


TEST_F(LDATest, LoadAccumulator_Immediate) {
    
}

