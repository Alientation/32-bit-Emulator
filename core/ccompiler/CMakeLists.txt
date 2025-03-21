cmake_minimum_required(VERSION 3.15)
project(ccompiler LANGUAGES C)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_C_FLAGS "-Wall -Wextra ")
set(CMAKE_C_FLAGS_DEBUG "-g")

# Use -flto only on non-Windows platforms (MinGW has issues with aggressive LTO)
if(NOT WIN32)
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O3 -flto=8")
    set(CMAKE_C_FLAGS_RELEASE "-O3 -flto=8")
else()
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O3")
    set(CMAKE_C_FLAGS_RELEASE "-O3")
endif()

# create final executable
add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
    src/main.c
    src/ccompiler.c
    src/lexer.c
    src/parser.c
    src/codegen.c
)

# Link POSIX regex library if necessary
if(WIN32)
    # On Windows, explicitly link the regex library (MinGW/Cygwin)
    target_link_libraries(${PROJECT_NAME} regex)
endif()

# sets the search paths for the include files after installation
# as well as during when building the library (as these may differ)
# this allows the library itself and users to #include the library headers
target_include_directories(
    ${PROJECT_NAME}
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Output a summary message for clarity
message(STATUS "Building ${PROJECT_NAME} on ${CMAKE_SYSTEM_NAME}")