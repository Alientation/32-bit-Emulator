#include <gtest/gtest.h>

#include <AlienCPUTest.h>

class LDATest : public testing::Test {
    AlienCPU cpu;

    virtual void SetUp() {
        cpu.Reset();
    }

    virtual void TearDown() {

    }



};