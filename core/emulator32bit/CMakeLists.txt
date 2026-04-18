cmake_minimum_required(VERSION 3.15)
project(emulator32bit VERSION 0.0.1 LANGUAGES CXX)

add_library(emulator32bit STATIC
    src/emulator32bit.cpp
    src/disassembler.cpp
    src/instructions.cpp
    src/software_interrupt.cpp
    src/memory.cpp
    src/virtual_memory.cpp
    src/kernel/better_virtual_memory.cpp
    src/system_bus.cpp
    src/disk.cpp
    src/fbl.cpp
    src/kernel/fbl_inmemory.cpp
    src/kernel/process.cpp
    src/kernel/malloc.cpp
    src/timer.cpp
)

target_include_directories(emulator32bit
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(emulator32bit PUBLIC util)
target_compile_features(emulator32bit PUBLIC cxx_std_20)

target_compile_options(emulator32bit PRIVATE
    -Wall -Wextra -Wpedantic
    $<$<CONFIG:Debug>:-g --coverage>
    $<$<CONFIG:RelWithDebInfo>:-O3 -flto=auto>
    $<$<CONFIG:Release>:-O3 -flto=auto>
)

target_link_options(emulator32bit PRIVATE
    $<$<CONFIG:Debug>:--coverage>
    $<$<CONFIG:RelWithDebInfo>:-flto=auto>
    $<$<CONFIG:Release>:-flto=auto>
)

set_target_properties(emulator32bit PROPERTIES DEBUG_POSTFIX "d")

include(GNUInstallDirs)
install(TARGETS emulator32bit
    EXPORT emulator32bit-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN "*.h")
install(EXPORT emulator32bit-targets
    NAMESPACE emulator32bit::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/emulator32bit
)

add_subdirectory(tests)