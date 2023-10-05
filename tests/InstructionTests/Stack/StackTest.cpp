#include <gtest/gtest.h>

#include <AlienCPUTest.h>

class StackTest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.Reset();
    }

    virtual void TearDown() {

    }
};


// TODO
