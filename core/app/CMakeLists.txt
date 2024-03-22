cmake_minimum_required(VERSION 3.15)
project(emulator-app LANGUAGES CXX)

add_custom_target(version 
    ${CMAKE_COMMAND} -D SRC=${CMAKE_SOURCE_DIR}/Version.h.in
                        -D DST=${CMAKE_SOURCE_DIR}/Version.h
                        -P ${CMAKE_SOURCE_DIR}/cmake/GenerateVersion.cmake
)

find_package(util REQUIRED CONFIG)
find_package(assembler REQUIRED CONFIG)
find_package(emulator6502 REQUIRED CONFIG)

add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE main.cpp)

target_link_libraries(${PROJECT_NAME} PUBLIC util::util assembler::assembler emulator6502::emulator6502)
add_dependencies(${PROJECT_NAME} version)





# add_subdirectory(libs/googletest) # add googletest subdirectory
# include_directories(libs/googletest/include) # this is so we can #include <gtest/gtest.h>

# UNIT TEST EXECUTABLE
# add_executable(EmulatorTestExe ${EMULATOR_TEST_SOURCES})
# target_link_libraries(EmulatorTestExe PRIVATE gtest)
# target_link_libraries(EmulatorTestExe PRIVATE Emulator)
# target_link_libraries(EmulatorTestExe PRIVATE EmulatorTests)
# target_link_libraries(EmulatorTestExe PRIVATE Util)

# PERFORMANCE TEST EXECUTABLE
# add_executable(EmulatorPerformanceTestExe "tests/AlienCPUPerformanceTests.cpp")
# target_link_libraries(EmulatorPerformanceTestExe PRIVATE Emulator)
# target_link_libraries(EmulatorPerformanceTestExe PRIVATE EmulatorPerformanceTests)
# target_link_libraries(EmulatorPerformanceTestExe PRIVATE Util)

# ASSEMBLER V1 EXECUTABLE
# add_executable(AssemblerV1Exe "src/AlienCPUAssembler.cpp")
# target_link_libraries(AssemblerV1Exe PRIVATE Assembler_V1)
# target_link_libraries(AssemblerV1Exe PRIVATE Emulator)
# target_link_libraries(AssemblerV1Exe PRIVATE Util)

# ASSEMBLER V2 EXECUTABLE
# add_executable(AssemblerV2Exe "src/Assembler/Assembler.cpp")
# target_link_libraries(AssemblerV2Exe PRIVATE Assembler_V2)
# target_link_libraries(AssemblerV2Exe PRIVATE Util)

# TEST
# add_executable(AssemblerV3Test "tests/AssemblerV3Tests/AssemblerTests.cpp")
# target_link_libraries(AssemblerV3Test PRIVATE Assembler_V3)
# target_link_libraries(AssemblerV3Test PRIVATE Util)

# enable_testing() #to  discover tests in test explorer 

# include(GoogleTest)
# gtest_discover_tests(AlienCPUTests) # discovers tests by asking the compiled test executable to enumerate its tests