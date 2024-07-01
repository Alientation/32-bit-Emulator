cmake_minimum_required(VERSION 3.15)
project(library_tester LANGUAGES CXX)

# GoogleTest requires at least C++14
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  googletest
  DOWNLOAD_EXTRACT_TIMESTAMP OFF
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# get required libraries
find_package(util REQUIRED CONFIG)
find_package(assembler REQUIRED CONFIG)
find_package(emulator6502 REQUIRED CONFIG)

# Link gtest to the tests.
enable_testing()
add_executable(emulator6502_tests)
target_sources(emulator6502_tests PRIVATE
    ./emulator6502_tests/AlienCPUTests.cpp

    # instruction tests
    # ./emulator6502_tests/InstructionTests/AddressingModes/<todo>
    ./emulator6502_tests/InstructionTests/Arithmetic/ADCTest.cpp
    ./emulator6502_tests/InstructionTests/Arithmetic/DECTest.cpp
    ./emulator6502_tests/InstructionTests/Arithmetic/DEXTest.cpp
    ./emulator6502_tests/InstructionTests/Arithmetic/DEYTest.cpp
    ./emulator6502_tests/InstructionTests/Arithmetic/INCTest.cpp
    ./emulator6502_tests/InstructionTests/Arithmetic/INXTest.cpp
    ./emulator6502_tests/InstructionTests/Arithmetic/INYTest.cpp
    ./emulator6502_tests/InstructionTests/Arithmetic/SBCTest.cpp

    ./emulator6502_tests/InstructionTests/Flag/CLCTest.cpp
    ./emulator6502_tests/InstructionTests/Flag/CLDTest.cpp
    ./emulator6502_tests/InstructionTests/Flag/CLITest.cpp
    ./emulator6502_tests/InstructionTests/Flag/CLVTest.cpp
    ./emulator6502_tests/InstructionTests/Flag/SECTest.cpp
    ./emulator6502_tests/InstructionTests/Flag/SEDTest.cpp
    ./emulator6502_tests/InstructionTests/Flag/SEITest.cpp

    ./emulator6502_tests/InstructionTests/Logic/ANDTest.cpp
    ./emulator6502_tests/InstructionTests/Logic/EORTest.cpp
    ./emulator6502_tests/InstructionTests/Logic/ORATest.cpp

    ./emulator6502_tests/InstructionTests/ShiftAndRotate/ASLTest.cpp
    ./emulator6502_tests/InstructionTests/ShiftAndRotate/LSRTest.cpp
    ./emulator6502_tests/InstructionTests/ShiftAndRotate/ROLTest.cpp
    ./emulator6502_tests/InstructionTests/ShiftAndRotate/RORTest.cpp

    ./emulator6502_tests/InstructionTests/Stack/PHATest.cpp
    ./emulator6502_tests/InstructionTests/Stack/PHPTest.cpp
    ./emulator6502_tests/InstructionTests/Stack/PLATest.cpp
    ./emulator6502_tests/InstructionTests/Stack/PLPTest.cpp

    ./emulator6502_tests/InstructionTests/Transfer/LDATest.cpp
    ./emulator6502_tests/InstructionTests/Transfer/LDXTest.cpp
    ./emulator6502_tests/InstructionTests/Transfer/LDYTest.cpp
    ./emulator6502_tests/InstructionTests/Transfer/STATest.cpp
    ./emulator6502_tests/InstructionTests/Transfer/STXTest.cpp
    ./emulator6502_tests/InstructionTests/Transfer/STYTest.cpp
    ./emulator6502_tests/InstructionTests/Transfer/TAXTest.cpp
    ./emulator6502_tests/InstructionTests/Transfer/TAYTest.cpp
    ./emulator6502_tests/InstructionTests/Transfer/TSXTest.cpp
    ./emulator6502_tests/InstructionTests/Transfer/TXATest.cpp
    ./emulator6502_tests/InstructionTests/Transfer/TXSTest.cpp
    ./emulator6502_tests/InstructionTests/Transfer/TYATest.cpp

    # internal emulator tests
    ./emulator6502_tests/InternalTests/MemoryAddressingTest.cpp
    ./emulator6502_tests/InternalTests/ResetTest.cpp
    ./emulator6502_tests/InternalTests/StackTest.cpp
    ./emulator6502_tests/InternalTests/StatusFlagTest.cpp
    ./emulator6502_tests/InternalTests/ValidInstructionTest.cpp

    # performance tests
    # ./emulator6502_tests/PerformanceTests/AlienCPUPerformanceTests.cpp
    # ./emulator6502_tests/PerformanceTests/StackPerformanceTest.cpp
)
target_include_directories(
    emulator6502_tests
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(emulator6502_tests PUBLIC util::util assembler::assembler emulator6502::emulator6502 GTest::gtest_main)

add_executable(assembler_tests)
target_sources(assembler_tests PRIVATE
    ./assembler_tests/AssemblerTests.cpp
)
target_include_directories(
    assembler_tests
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(assembler_tests PUBLIC util::util assembler::assembler emulator6502::emulator6502 GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(emulator6502_tests)
gtest_discover_tests(assembler_tests)