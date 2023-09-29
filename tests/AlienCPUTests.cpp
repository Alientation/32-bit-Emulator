#include <gtest/gtest.h>
#include <iostream>
#include <../src/Motherboard/AlienCPU6502.h>

int main(int argc, char* argv[])
{
    AlienCPU cpu;

    std::cout << "AlienCPU v" << AlienCPU::VERSION << " Tests" << std::endl;
    std::cout << "RAM: " << sizeof(cpu.ram.Data) << " bytes" << std::endl;

    cpu.Start(1);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}