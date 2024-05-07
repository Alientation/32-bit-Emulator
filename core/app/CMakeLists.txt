cmake_minimum_required(VERSION 3.15)
project(emulator-app LANGUAGES CXX)

# get project version from git
add_custom_target(version 
    ${CMAKE_COMMAND} -D SRC=${CMAKE_SOURCE_DIR}/Version.h.in
                        -D DST=${CMAKE_SOURCE_DIR}/Version.h
                        -P ${CMAKE_SOURCE_DIR}/cmake/GenerateVersion.cmake
)

# get required libraries
find_package(util REQUIRED CONFIG)
find_package(assembler REQUIRED CONFIG)
find_package(emulator6502 REQUIRED CONFIG)

# create final executable
add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE main.cpp)

# link libraries
target_link_libraries(${PROJECT_NAME} PUBLIC util::util assembler::assembler emulator6502::emulator6502)
add_dependencies(${PROJECT_NAME} version)
