cmake_minimum_required(VERSION 3.15)
project(ccompiler LANGUAGES C)

# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()


# Create target
add_library(ccompiler STATIC
    src/ccompiler.c
    src/lexer.c
    src/parser.c
    src/codegen.c
    src/stringbuffer.c
    src/massert.c
    src/util.c
)

# Create final executable
add_executable(ccompiler_exec
    src/main.c
)

# Apply warnings and flags to both the library and the executable
foreach(tgt ccompiler ccompiler_exec)
    target_compile_options(${tgt} PRIVATE
        -Wall
        -Wextra
    )

    target_compile_options(${tgt} PRIVATE
        $<$<CONFIG:Debug>:-g --coverage>
        $<$<CONFIG:RelWithDebInfo>:-O3 -flto=8>
        $<$<CONFIG:Release>:-O3 -flto=8>
    )

    if(WIN32)
        # Windows LTO workaround
        target_compile_options(${tgt} PRIVATE
            $<$<CONFIG:RelWithDebInfo>:-O3>
            $<$<CONFIG:Release>:-O3>
        )
    endif()
endforeach()

# Include directories
target_include_directories(ccompiler
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Link libraries
target_link_libraries(ccompiler_exec PRIVATE ccompiler)

if(WIN32)
    # MinGW/Cygwin regex
    target_link_libraries(ccompiler PRIVATE regex)
endif()

add_subdirectory(tests)

# Output a summary message for clarity
message(STATUS "Building ccompiler on ${CMAKE_SYSTEM_NAME}")