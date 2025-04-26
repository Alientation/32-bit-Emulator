cmake_minimum_required(VERSION 3.15)
project(assembler_tester LANGUAGES CXX)

# GoogleTest requires at least C++14
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# link gtest
enable_testing()

add_executable(assembler_tests
	./assembler_test.cpp

	./preprocessor_test/include.cpp
	./preprocessor_test/macro.cpp
	./preprocessor_test/define.cpp
	./preprocessor_test/conditional.cpp
)

target_include_directories(
	assembler_tests
	PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(assembler_tests PRIVATE util emulator32bit assembler GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(assembler_tests)