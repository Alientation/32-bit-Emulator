cmake_minimum_required(VERSION 3.15)
project(emulator_app VERSION 0.0.1 LANGUAGES CXX)

# Version header generation
set(VERSION_HEADER "${CMAKE_SOURCE_DIR}/src/version.h")

# Define custom command to generate version header
add_custom_command(
    OUTPUT ${VERSION_HEADER}
    COMMAND ${CMAKE_COMMAND} -D SRC=${CMAKE_SOURCE_DIR}/src/version.h.in
                             -D DST=${VERSION_HEADER}
                             -P ${CMAKE_SOURCE_DIR}/cmake/GenerateVersion.cmake
    COMMENT "Generating version header"
    VERBATIM
)

# Wrap it in a custom target
add_custom_target(generate_version_header
    DEPENDS ${VERSION_HEADER}
)

# Main executable
add_executable(${PROJECT_NAME}
    src/main.cpp
    ${VERSION_HEADER}   # Ensure dependency is visible
)

# Include directories (PRIVATE)
target_include_directories(${PROJECT_NAME} PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# Link required libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    util
    emulator32bit
    assembler
)

# Ensure version header is generated before building
add_dependencies(${PROJECT_NAME} generate_version_header)

# Compiler flags per target
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall -Wextra -Wpedantic
    $<$<CONFIG:Debug>:-g --coverage>
    $<$<CONFIG:RelWithDebInfo>:-O3 -flto=auto>
    $<$<CONFIG:Release>:-O3 -flto=auto>
)

# Optional: additional executables (kernel)
# add_executable(kernel src/kernel.cpp ${VERSION_HEADER})
# target_include_directories(kernel PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
# target_link_libraries(kernel PRIVATE util emulator32bit assembler)
# add_dependencies(kernel generate_version_header)
