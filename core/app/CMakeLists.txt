cmake_minimum_required(VERSION 3.15)
project(emulator-app LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra ")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -flto=auto")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -flto=auto")

# Generate version header
add_custom_target(version
    ${CMAKE_COMMAND} -D SRC=${CMAKE_SOURCE_DIR}/src/version.h.in
                     -D DST=${CMAKE_SOURCE_DIR}/src/version.h
                     -P ${CMAKE_SOURCE_DIR}/cmake/GenerateVersion.cmake
)

# Add sibling directories
add_subdirectory(../util ${CMAKE_BINARY_DIR}/util)
add_subdirectory(../emulator32bit ${CMAKE_BINARY_DIR}/emulator32bit)
add_subdirectory(../assembler ${CMAKE_BINARY_DIR}/assembler)

# Main executable
add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE src/main.cpp)

target_include_directories(
    ${PROJECT_NAME}
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
    util
    emulator32bit
    assembler
)

add_dependencies(${PROJECT_NAME} version)


# create executable to run the kernel on the emulator
# add_executable(kernel)
# target_sources(kernel PRIVATE src/kernel.cpp)
# target_link_libraries(kernel PUBLIC util::util emulator32bit::emulator32bit assembler::assembler)
# add_dependencies(kernel version)