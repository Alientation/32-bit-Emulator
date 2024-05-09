cmake_minimum_required(VERSION 3.15)
project(test-app LANGUAGES CXX)

# test executable
find_package(util REQUIRED CONFIG)
find_package(emulator32bit REQUIRED CONFIG)

add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE Test.cpp)
target_link_libraries(${PROJECT_NAME} PUBLIC util::util emulator32bit::emulator32bit)