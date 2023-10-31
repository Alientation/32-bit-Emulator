cmake_minimum_required(VERSION 3.12)
project(AlienCPU)

file( GLOB ALIENCPU_SOURCES 
    "src/AlienCPU/AlienCPU.cpp"
    "src/Device/Input/Keyboard.cpp"
    "src/Device/Input/Mouse.cpp"
    "src/Motherboard/Memory/RAM.cpp"
    "src/Motherboard/Memory/ROM.cpp"
    "src/Motherboard/AlienCPU.cpp"
    "src/Motherboard/AddressingModesUtil.cpp"
    "src/Motherboard/InstructionLinker.cpp"
    "src/Motherboard/Instructions.cpp"
    "src/Motherboard/BIOS.cpp"
    "src/Motherboard/Motherboard.cpp"
    "src/Device/Output/Monitor.cpp"
    "src/Device/Output/Speaker.cpp"
    "src/Device/Storage/HDD.cpp"
    "src/Device/Storage/SSD.cpp"
    "src/Device/GPU.cpp"
    "src/Device/PSU.cpp"

    "src/AlienCPUAssembler.cpp"
    "src/AssemblerDirectives.cpp"

    "src/Assembler/Assembler.cpp"
    "src/Assembler/Directives.cpp"
    "src/Assembler/Linker.cpp"
)
file (GLOB ALIENCPU_HEADERS 
    "src/AlienCPU/AlienCPU.h"
    "src/Device/Input/Keyboard.h"
    "src/Device/Input/Mouse.h"
    "src/Motherboard/Memory/RAM.h"
    "src/Motherboard/Memory/ROM.h"
    "src/Motherboard/AlienCPU.h"
    "src/Motherboard/BIOS.h"
    "src/Motherboard/Motherboard.h"
    "src/Device/Output/Monitor.h"
    "src/Device/Output/Speaker.h"
    "src/Device/Storage/HDD.h"
    "src/Device/Storage/SSD.h"
    "src/Device/GPU.h"
    "src/Device/PSU.h"
    "src/Types.h"

    "src/AlienCPUAssembler.h"

    "src/Assembler/Assembler.h"
    "src/Assembler/Linker.h"
    
    "src/ConsoleColor.h"
)

file (GLOB ALIENCPUTEST_SOURCES 
    "tests/AlienCPUTests.cpp"

    "tests/InstructionTests/Transfer/LDATest.cpp"
    "tests/InstructionTests/Transfer/LDXTest.cpp"
    "tests/InstructionTests/Transfer/LDYTest.cpp"
    "tests/InstructionTests/Transfer/STATest.cpp"
    "tests/InstructionTests/Transfer/STXTest.cpp"
    "tests/InstructionTests/Transfer/STYTest.cpp"
    "tests/InstructionTests/Transfer/TAXTest.cpp"
    "tests/InstructionTests/Transfer/TAYTest.cpp"
    "tests/InstructionTests/Transfer/TSXTest.cpp"
    "tests/InstructionTests/Transfer/TXATest.cpp"
    "tests/InstructionTests/Transfer/TXSTest.cpp"
    "tests/InstructionTests/Transfer/TYATest.cpp"

    "tests/InstructionTests/Stack/PHATest.cpp"
    "tests/InstructionTests/Stack/PHPTest.cpp"
    "tests/InstructionTests/Stack/PLATest.cpp"
    "tests/InstructionTests/Stack/PLPTest.cpp"

    "tests/InstructionTests/Arithmetic/DECTest.cpp"
    "tests/InstructionTests/Arithmetic/DEXTest.cpp"
    "tests/InstructionTests/Arithmetic/DEYTest.cpp"
    "tests/InstructionTests/Arithmetic/INCTest.cpp"
    "tests/InstructionTests/Arithmetic/INXTest.cpp"
    "tests/InstructionTests/Arithmetic/INYTest.cpp"
    "tests/InstructionTests/Arithmetic/ADCTest.cpp"
    "tests/InstructionTests/Arithmetic/SBCTest.cpp"

    "tests/InstructionTests/Logic/ANDTest.cpp"
    "tests/InstructionTests/Logic/EORTest.cpp"
    "tests/InstructionTests/Logic/ORATest.cpp"

    "tests/InstructionTests/ShiftAndRotate/ASLTest.cpp"
    "tests/InstructionTests/ShiftAndRotate/LSRTest.cpp"
    "tests/InstructionTests/ShiftAndRotate/ROLTest.cpp"
    "tests/InstructionTests/ShiftAndRotate/RORTest.cpp"

    "tests/InstructionTests/Flag/CLCTest.cpp"
    "tests/InstructionTests/Flag/CLDTest.cpp"
    "tests/InstructionTests/Flag/CLITest.cpp"
    "tests/InstructionTests/Flag/CLVTest.cpp"
    "tests/InstructionTests/Flag/SECTest.cpp"
    "tests/InstructionTests/Flag/SEDTest.cpp"
    "tests/InstructionTests/Flag/SEITest.cpp"

    "tests/InternalTests/ValidInstructionTest.cpp"
    "tests/InternalTests/StatusFlagTest.cpp"
    "tests/InternalTests/MemoryAddressingTest.cpp"
    "tests/InternalTests/StackTest.cpp"
    "tests/InternalTests/ResetTest.cpp"
    
    "tests/PerformanceTests/StackPerformanceTest.cpp"
)

file (GLOB ALIENCPUTEST_HEADERS 
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

# ASSEMBLER EXECUTABLE
add_executable(AlienCPUAssembler "src/AlienCPUAssembler.cpp")
target_link_libraries(AlienCPUAssembler PRIVATE AlienCPUV1) # link main library to the assembler executable

enable_testing() #to  discover tests in test explorer 

include(GoogleTest)
gtest_discover_tests(AlienCPUTests) # discovers tests by asking the compiled test executable to enumerate its tests