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
	./Emulator32bitTest.cpp

	./InstructionTests/add_test.cpp
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