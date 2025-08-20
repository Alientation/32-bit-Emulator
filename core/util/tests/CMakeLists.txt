cmake_minimum_required(VERSION 3.15)
project(util_tests LANGUAGES CXX)

# GoogleTest requires at least C++14
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

add_executable(${PROJECT_NAME}
	./util_test.cpp

    ./util_tests/short_string_test.cpp
)

target_link_libraries(
    ${PROJECT_NAME}
	PRIVATE
	util
	emulator32bit
	GTest::gtest_main)

# GCC, use gcov to display code coverage
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	target_link_libraries(${PROJECT_NAME}
		PRIVATE
		gcov)
endif()

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME})