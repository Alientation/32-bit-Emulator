cmake_minimum_required(VERSION 3.12) # version can be different
project(AlienCPU) #name of your project

#include_directories(src/Input src/Motherboard/Memory src/Motherboard src/Output src/Storage src tests)

file(GLOB all_SRCS
    "src/Input/Keyboard.cpp"
    "src/Input/Keyboard.h"
    "src/Input/Mouse.cpp"
    "src/Input/Mouse.h"
    "src/Motherboard/Memory/RAM.cpp"
    "src/Motherboard/Memory/RAM.h"
    "src/Motherboard/Memory/ROM.cpp"
    "src/Motherboard/Memory/ROM.h"
    "src/Motherboard/AlienCPU.cpp"
    "src/Motherboard/AlienCPU.h"
    "src/Motherboard/BIOS.cpp"
    "src/Motherboard/BIOS.h"
    "src/Motherboard/Motherboard.cpp"
    "src/Motherboard/Motherboard.h"
    "src/Output/Monitor.cpp"
    "src/Output/Monitor.h"
    "src/Output/Speaker.cpp"
    "src/Output/Speaker.h"
    "src/Storage/HDD.cpp"
    "src/Storage/HDD.h"
    "src/Storage/SSD.cpp"
    "src/Storage/SSD.h"
    "src/GPU.cpp"
    "src/GPU.h"
    "src/PSU.cpp"
    "src/PSU.h"
    "src/Types.h"

    "tests/AlienCPUTests.cpp"
    "tests/AlienCPUTest.h"

    "tests/InstructionTests/Transfer/LDATest.cpp"
    "tests/InstructionTests/Transfer/LDXTest.cpp"
    "tests/InstructionTests/Transfer/LDYTest.cpp"
    "tests/InstructionTests/Transfer/STATest.cpp"
    "tests/InstructionTests/Transfer/STXTest.cpp"
    "tests/InstructionTests/Transfer/STYTest.cpp"
    "tests/InstructionTests/Transfer/TransferRegistersTest.cpp"
)

include_directories(src tests)


add_subdirectory(libs/googletest) # add googletest subdirectory
include_directories(libs/googletest/include) # this is so we can #include <gtest/gtest.h>

add_executable(AlienCPUTests ${all_SRCS}) # add this executable

target_link_libraries(AlienCPUTests PRIVATE gtest) # link google test to this executable

enable_testing() #to  discover tests in test explorer 

include(GoogleTest)
gtest_discover_tests(AlienCPUTests) # discovers tests by asking the compiled test executable to enumerate its tests