cmake_minimum_required(VERSION 3.15)
project(ccompiler_test LANGUAGES CXX)

# GoogleTest requires at least C++14
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# link gtest
enable_testing()

add_executable(${PROJECT_NAME}
    ./ccompiler_test.cpp

    ./util_test/stringbuffer_test.cpp
    ./ccompiler_test/lexer_test.cpp
)

target_include_directories(
	${PROJECT_NAME}
	PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    ccompiler
    GTest::gtest_main)

# GCC, use gcov to display code coverage
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	target_link_libraries(${PROJECT_NAME}
	    PRIVATE
	    gcov)
endif()

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME})