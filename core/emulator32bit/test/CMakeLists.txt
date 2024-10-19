cmake_minimum_required(VERSION 3.15)
project(library_tester LANGUAGES CXX)

# GoogleTest requires at least C++14
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
	googletest
	URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# get required libraries
find_package(util REQUIRED CONFIG)
find_package(emulator32bit REQUIRED CONFIG)

# link gtest
enable_testing()
add_executable(emulator32bit_tests)
target_sources(emulator32bit_tests PRIVATE
	# add test source files here
	./emulator32bit_test.cpp

	./emulator_tests/emulator_test.cpp
	./emulator_tests/fbl_inmemory_test.cpp

	./instruction_tests/hlt_test.cpp
	./instruction_tests/add_test.cpp
	./instruction_tests/sub_test.cpp
	./instruction_tests/rsb_test.cpp
	./instruction_tests/adc_test.cpp
	./instruction_tests/sbc_test.cpp
	./instruction_tests/rsc_test.cpp
	./instruction_tests/mul_test.cpp
	./instruction_tests/umull_test.cpp
	./instruction_tests/smull_test.cpp
	./instruction_tests/and_test.cpp
	./instruction_tests/orr_test.cpp
	./instruction_tests/eor_test.cpp
	./instruction_tests/bic_test.cpp
	./instruction_tests/lsl_test.cpp
	./instruction_tests/lsr_test.cpp
	./instruction_tests/asr_test.cpp
	./instruction_tests/ror_test.cpp
	./instruction_tests/cmp_test.cpp
	./instruction_tests/cmn_test.cpp
	./instruction_tests/tst_test.cpp
	./instruction_tests/teq_test.cpp
	./instruction_tests/mov_test.cpp
	./instruction_tests/mvn_test.cpp
	./instruction_tests/ldr_test.cpp
)

target_include_directories(
	emulator32bit_tests
	PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(emulator32bit_tests PUBLIC util::util emulator32bit::emulator32bit GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(emulator32bit_tests)

add_executable(quick_test)
target_sources(quick_test PRIVATE
	./test.cpp
)

target_include_directories(
	quick_test
	PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(quick_test PUBLIC util::util emulator32bit::emulator32bit)