cmake_minimum_required(VERSION 3.15)
project(assembler_tester LANGUAGES CXX)

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
find_package(assembler REQUIRED CONFIG)

# link gtest
enable_testing()
add_executable(assembler_tests)
target_sources(assembler_tests PRIVATE
	# add test source files here
	./assembler_test.cpp

	./preprocessor_test/include.cpp
	./preprocessor_test/macro.cpp
)

target_include_directories(
	assembler_tests
	PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(assembler_tests PUBLIC util::util emulator32bit::emulator32bit assembler::assembler GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(assembler_tests)