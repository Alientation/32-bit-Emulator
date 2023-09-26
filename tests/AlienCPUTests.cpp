#include <gtest/gtest.h>
#include <iostream>
#include <../src/Motherboard/AlienCPU.h>

int main(int argc, char* argv[])
{
    AlienCPU cpu;

    std::cout << "AlienCPU v" << AlienCPU::VERSION << " Tests" << std::endl;
    std::cout << "RAM: " << sizeof(cpu.RAM.Data) << " bytes" << std::endl;

    cpu.Start(1);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}