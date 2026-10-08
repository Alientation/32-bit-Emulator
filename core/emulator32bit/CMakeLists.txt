add_library(emulator32bit STATIC
    src/emulator32bit.cpp
    src/disassembler.cpp
    src/instructions.cpp
    src/software_interrupt.cpp
    src/memory.cpp
    src/virtual_memory.cpp
    src/system_bus.cpp
    src/memory_port.cpp
    src/disk.cpp
    src/fbl.cpp
    src/symbols.cpp
    src/debugger.cpp
    src/devices.cpp
)
add_library(aemu::emulator32bit ALIAS emulator32bit)

target_include_directories(emulator32bit
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(emulator32bit PUBLIC cxx_std_20)
target_link_libraries(emulator32bit PUBLIC aemu::util)
aemu_target_defaults(emulator32bit)

# Debug postfix
set_target_properties(emulator32bit PROPERTIES DEBUG_POSTFIX "d")

# Executable to take in cmd line arguments. It also links the assembler library because the .bexe
# reader (LoadExecutable/ObjectFile) lives there; the libraries themselves stay layered.
add_executable(emu32 src/cli.cpp)
target_link_libraries(emu32 PRIVATE aemu::emulator32bit aemu::assembler cxxopts::cxxopts)
aemu_target_defaults(emu32)

# Installation rules
include(GNUInstallDirs)
install(TARGETS emulator32bit
    EXPORT emulator32bit-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS emu32
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN "*.h")

install(EXPORT emulator32bit-targets
    NAMESPACE emulator32bit::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/emulator32bit
)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
