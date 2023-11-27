cmake_minimum_required(VERSION 3.12)
project(AlienCPU)

file( GLOB UTIL_HEADERS
    "src/util/Types.h"
	"src/util/ConsoleColor.h"
	"src/util/Logger.h"
	"src/util/StringUtil.h"
	"src/util/File.h"
	"src/util/Directory.h"
    "src/AssemblerV3/AssemblyType.h"
)

file ( GLOB UTIL_SOURCES
	"src/util/File.cpp"
	"src/util/Directory.cpp"
)

file( GLOB EMULATOR_SOURCES
    "src/AlienCPU/AlienCPU.cpp"
    "src/Device/Input/Keyboard.cpp"
    "src/Device/Input/Mouse.cpp"
    "src/Emulator/Memory/RAM.cpp"
    "src/Emulator/Memory/ROM.cpp"
    "src/Emulator/AlienCPU.cpp"
    "src/Emulator/AddressingModesUtil.cpp"
    "src/Emulator/InstructionLinker.cpp"
    "src/Emulator/Instructions.cpp"
    "src/Emulator/BIOS.cpp"
    "src/Emulator/Motherboard.cpp"
    "src/Device/Output/Monitor.cpp"
    "src/Device/Output/Speaker.cpp"
    "src/Device/Storage/HDD.cpp"
    "src/Device/Storage/SSD.cpp"
    "src/Device/GPU.cpp"
    "src/Device/PSU.cpp"
)

file ( GLOB EMULATOR_HEADERS
    "src/AlienCPU/AlienCPU.h"
    "src/Device/Input/Keyboard.h"
    "src/Device/Input/Mouse.h"
    "src/Emulator/Memory/RAM.h"
    "src/Emulator/Memory/ROM.h"
    "src/Emulator/AlienCPU.h"
    "src/Emulator/BIOS.h"
    "src/Emulator/Motherboard.h"
    "src/Device/Output/Monitor.h"
    "src/Device/Output/Speaker.h"
    "src/Device/Storage/HDD.h"
    "src/Device/Storage/SSD.h"
    "src/Device/GPU.h"
    "src/Device/PSU.h"
)

file( GLOB ASSEMBLER_V1_SOURCES
	"src/AlienCPUAssembler.cpp"
	"src/AssemblerDirectives.cpp"
)

file( GLOB ASSEMBLER_V1_HEADERS
	"src/AlienCPUAssembler.h"
)

file( GLOB ASSEMBLER_V2_SOURCES
	"src/Assembler/Assembler.cpp"
	"src/Assembler/Directives.cpp"
	"src/Assembler/Linker.cpp"
)

file( GLOB ASSEMBLER_V2_HEADERS
	"src/Assembler/Assembler.h"
	"src/Assembler/Linker.h"
)


file( GLOB ASSEMBLER_V3_SOURCES
	"src/AssemblerV3/Build.cpp"
    "src/AssemblerV3/Tokenizer.cpp"
	"src/AssemblerV3/Preprocess/PreprocessorV3.cpp"
	"src/AssemblerV3/Assemble/AssemblerV3.cpp"
	"src/AssemblerV3/Assemble/AssemblerV3Debug.cpp"
	"src/AssemblerV3/Assemble/DirectivesV3.cpp"
	"src/AssemblerV3/Assemble/ObjectFileV3.cpp"
	"src/AssemblerV3/Link/LinkerV3.cpp"
	"src/AssemblerV3/Link/ExecutableFileV3.cpp"
	
)
file (GLOB ASSEMBLER_V3_HEADERS
	"src/AssemblerV3/Build.h"
    "src/AssemblerV3/Tokenizer.h"
	"src/AssemblerV3/Assemble/AssemblerV3.h"
	"src/AssemblerV3/Link/ExecutableFileV3.h"
	"src/AssemblerV3/Link/LinkerV3.h"
	"src/AssemblerV3/Assemble/ObjectFileV3.h"
	"src/AssemblerV3/Preprocess/PreprocessorV3.h"
)

file (GLOB EMULATOR_TEST_SOURCES 
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

file (GLOB EMULATOR_TEST_HEADERS 
    "tests/AlienCPUTest.h"
)

file (GLOB EMULATOR_PERFORMANCETEST_SOURCES
    "tests/AlienCPUPerformanceTests.cpp"
)

file (GLOB EMULATOR_PERFORMANCETEST_HEADERS

)

file (GLOB ASSEMBLER_TEST_SOURCES 
	"tests/AssemblerV3Tests/AssemblerTests.cpp"
)

file (GLOB ASSEMBLER_TEST_HEADERS

)


# add_library(Emulator STATIC ${EMULATOR_SOURCES} ${EMULATOR_HEADERS})
# add_library(Assembler_V1 STATIC ${ASSEMBLER_V1_SOURCES} ${ASSEMBLER_V1_HEADERS})
# add_library(Assembler_V2 STATIC ${ASSEMBLER_V2_SOURCES} ${ASSEMBLER_V2_HEADERS})
add_library(Assembler_V3 STATIC ${ASSEMBLER_V3_SOURCES} ${ASSEMBLER_V3_HEADERS})
# add_library(EmulatorTests STATIC ${EMULATOR_TEST_SOURCES} ${EMULATOR_TEST_HEADERS})
# add_library(EmulatorPerformanceTests STATIC ${EMULATOR_PERFORMANCETEST_SOURCES} ${EMULATOR_PERFORMANCETEST_HEADERS})
add_library(AssemblerTests STATIC ${ASSEMBLER_TEST_SOURCES} ${ASSEMBLER_TEST_HEADERS})
add_library(Util STATIC  ${UTIL_HEADERS} ${UTIL_SOURCES})

include_directories(src tests)


add_subdirectory(libs/googletest) # add googletest subdirectory
include_directories(libs/googletest/include) # this is so we can #include <gtest/gtest.h>

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
add_executable(AssemblerV3Test "tests/AssemblerV3Tests/AssemblerTests.cpp")
target_link_libraries(AssemblerV3Test PRIVATE Assembler_V3)
target_link_libraries(AssemblerV3Test PRIVATE Util)

# enable_testing() #to  discover tests in test explorer 

# include(GoogleTest)
# gtest_discover_tests(AlienCPUTests) # discovers tests by asking the compiled test executable to enumerate its tests