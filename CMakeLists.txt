cmake_minimum_required(VERSION 3.12) # version can be different
project(AlienCPU) #name of your project

#include_directories(src/Input src/Motherboard/Memory src/Motherboard src/Output src/Storage src tests)

# file(GLOB all_SRCS
#     "src/AlienCPU/AlienCPU.cpp"
#     "src/AlienCPU/AlienCPU.h"

#     "src/Input/Keyboard.cpp"
#     "src/Input/Keyboard.h"
#     "src/Input/Mouse.cpp"
#     "src/Input/Mouse.h"
#     "src/Motherboard/Memory/RAM.cpp"
#     "src/Motherboard/Memory/RAM.h"
#     "src/Motherboard/Memory/ROM.cpp"
#     "src/Motherboard/Memory/ROM.h"
#     "src/Motherboard/AlienCPU.cpp"
#     "src/Motherboard/AlienCPU.h"
#     "src/Motherboard/BIOS.cpp"
#     "src/Motherboard/BIOS.h"
#     "src/Motherboard/Motherboard.cpp"
#     "src/Motherboard/Motherboard.h"
#     "src/Output/Monitor.cpp"
#     "src/Output/Monitor.h"
#     "src/Output/Speaker.cpp"
#     "src/Output/Speaker.h"
#     "src/Storage/HDD.cpp"
#     "src/Storage/HDD.h"
#     "src/Storage/SSD.cpp"
#     "src/Storage/SSD.h"
#     "src/GPU.cpp"
#     "src/GPU.h"
#     "src/PSU.cpp"
#     "src/PSU.h"
#     "src/Types.h"

#     "tests/AlienCPUTests.cpp"
#     "tests/AlienCPUTest.h"

#     "tests/InstructionTests/Transfer/LDATest.cpp"
#     "tests/InstructionTests/Transfer/LDXTest.cpp"
#     "tests/InstructionTests/Transfer/LDYTest.cpp"
#     "tests/InstructionTests/Transfer/STATest.cpp"
#     "tests/InstructionTests/Transfer/STXTest.cpp"
#     "tests/InstructionTests/Transfer/STYTest.cpp"
#     "tests/InstructionTests/Transfer/TransferRegistersTest.cpp"

#     "tests/InstructionTests/Stack/StackTest.cpp"

#     "tests/InternalTests/ValidInstructionTest.cpp"
#     "tests/InternalTests/StatusFlagTest.cpp"
#     "tests/InternalTests/MemoryAddressingTest.cpp"
#     "tests/InternalTests/StackTest.cpp"
#     "tests/InternalTests/ResetTest.cpp"
    
#     "tests/PerformanceTests/StackPerformanceTest.cpp"
# )
file( GLOB ALIENCPU_SOURCES 
    "src/AlienCPU/AlienCPU.cpp"
    "src/Input/Keyboard.cpp"
    "src/Input/Mouse.cpp"
    "src/Motherboard/Memory/RAM.cpp"
    "src/Motherboard/Memory/ROM.cpp"
    "src/Motherboard/AlienCPU.cpp"
    "src/Motherboard/BIOS.cpp"
    "src/Motherboard/Motherboard.cpp"
    "src/Output/Monitor.cpp"
    "src/Output/Speaker.cpp"
    "src/Storage/HDD.cpp"
    "src/Storage/SSD.cpp"
    "src/GPU.cpp"
    "src/PSU.cpp"
)
file (GLOB ALIENCPU_HEADERS 
    "src/AlienCPU/AlienCPU.h"
    "src/Input/Keyboard.h"
    "src/Input/Mouse.h"
    "src/Motherboard/Memory/RAM.h"
    "src/Motherboard/Memory/ROM.h"
    "src/Motherboard/AlienCPU.h"
    "src/Motherboard/BIOS.h"
    "src/Motherboard/Motherboard.h"
    "src/Output/Monitor.h"
    "src/Output/Speaker.h"
    "src/Storage/HDD.h"
    "src/Storage/SSD.h"
    "src/GPU.h"
    "src/PSU.h"
    "src/Types.h"
)

file (GLOB ALIENCPUTEST_SOURCES 
    "tests/AlienCPUTests.cpp"

    "tests/InstructionTests/Transfer/LDATest.cpp"
    "tests/InstructionTests/Transfer/LDXTest.cpp"
    "tests/InstructionTests/Transfer/LDYTest.cpp"
    "tests/InstructionTests/Transfer/STATest.cpp"
    "tests/InstructionTests/Transfer/STXTest.cpp"
    "tests/InstructionTests/Transfer/STYTest.cpp"
    "tests/InstructionTests/Transfer/TransferRegistersTest.cpp"

    "tests/InstructionTests/Stack/StackTest.cpp"

    "tests/InternalTests/ValidInstructionTest.cpp"
    "tests/InternalTests/StatusFlagTest.cpp"
    "tests/InternalTests/MemoryAddressingTest.cpp"
    "tests/InternalTests/StackTest.cpp"
    "tests/InternalTests/ResetTest.cpp"
    
    "tests/PerformanceTests/StackPerformanceTest.cpp"
)

file (GLOB ALIENCPUV1TEST_HEADERS 
    "tests/AlienCPUTest.h"
)

file (GLOB ALIENCPUPERFORMANCETEST_SOURCES
    "tests/AlienCPUPerformanceTests.cpp"
)

file (GLOB ALIENCPUPERFORMANCETEST_HEADERS

)


add_library(AlienCPUV1 STATIC ${ALIENCPU_SOURCES} ${ALIENCPU_HEADERS})
add_library(AlienCPUV1Tests STATIC ${ALIENCPUTEST_SOURCES} ${ALIENCPUTEST_HEADERS})
add_library(AlienCPUV1PerformanceTests STATIC ${ALIENCPUPERFORMANCETEST_SOURCES} ${ALIENCPUPERFORMANCETEST_HEADERS})

include_directories(src tests)


add_subdirectory(libs/googletest) # add googletest subdirectory
include_directories(libs/googletest/include) # this is so we can #include <gtest/gtest.h>

# UNIT TEST EXECUTABLE
add_executable(AlienCPUTests ${ALIENCPUTEST_SOURCES})
target_link_libraries(AlienCPUTests PRIVATE gtest) # link google test to the unit test executable
target_link_libraries(AlienCPUTests PRIVATE AlienCPUV1) # link main library to the unit test executable
target_link_libraries(AlienCPUTests PRIVATE AlienCPUV1Tests) # link test library to the unit test executable

# PERFORMANCE TEST EXECUTABLE
add_executable(AlienCPUPerformanceTests "tests/AlienCPUPerformanceTests.cpp")
target_link_libraries(AlienCPUPerformanceTests PRIVATE AlienCPUV1) # link main library to the performance test executable
target_link_libraries(AlienCPUPerformanceTests PRIVATE AlienCPUV1PerformanceTests) # link performance test library to the performance test executable

enable_testing() #to  discover tests in test explorer 

include(GoogleTest)
gtest_discover_tests(AlienCPUTests) # discovers tests by asking the compiled test executable to enumerate its tests