#include <gtest/gtest.h>
#include <iostream>

// #include <AlienCPUTest.h>

int main(int argc, char* argv[]) {
    // AlienCPU cpu;

    // std::cout << "AlienCPU v" << AlienCPU::VERSION << " Tests" << std::endl;
    // std::cout << "RAM: " << sizeof(cpu.motherboard.ram.data) << " bytes" << std::endl;

    // cpu.startCycles(0);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}