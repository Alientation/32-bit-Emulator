cmake_minimum_required(VERSION 3.15)
project(assembler VERSION 0.0.1 LANGUAGES CXX)

add_library(assembler STATIC
    src/assembler.cpp
    src/build.cpp
    src/directives.cpp
    src/instructions.cpp
    src/load_executable.cpp
    src/linker.cpp
    src/object_file.cpp
    src/static_library.cpp
    src/preprocessor.cpp
    src/tokenizer.cpp
)

target_include_directories(assembler
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(assembler PUBLIC cxx_std_20)

target_link_libraries(assembler PUBLIC util emulator32bit)

# Debug postfix
set_target_properties(assembler PROPERTIES DEBUG_POSTFIX "d")

# Executable
add_executable(basm src/main.cpp)
target_link_libraries(basm PRIVATE assembler)
target_compile_features(basm PRIVATE cxx_std_20)

# Compiler flags per target
foreach(tgt assembler basm)
    target_compile_options(${tgt} PRIVATE
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Debug>:-g --coverage>
        $<$<CONFIG:RelWithDebInfo>:-O3 -flto=auto>
        $<$<CONFIG:Release>:-O3 -flto=auto>
    )

    target_link_options(${tgt} PRIVATE
        $<$<CONFIG:Debug>:--coverage>
        $<$<CONFIG:RelWithDebInfo>:-flto=auto>
        $<$<CONFIG:Release>:-flto=auto>
    )
endforeach()

# Installation rules
include(GNUInstallDirs)

install(TARGETS assembler
    EXPORT assembler-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS basm
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN "*.h")

install(EXPORT assembler-targets
    NAMESPACE assembler::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/assembler
)

add_subdirectory(tests)