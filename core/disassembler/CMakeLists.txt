# The disassembler of the instruction set. It includes the headers of the encoding from the
# emulator library (encoding.h, opcodes.h, alu.h, fpu.h, all of them constants and inline
# functions) and needs nothing to link with it, so the emulator can use it.
add_library(disassembler STATIC
    src/disassembler.cpp
)
add_library(aemu::disassembler ALIAS disassembler)

target_include_directories(disassembler
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../emulator32bit/include
)

target_compile_features(disassembler PUBLIC cxx_std_20)
target_link_libraries(disassembler PUBLIC aemu::util)
aemu_target_defaults(disassembler)

set_target_properties(disassembler PROPERTIES DEBUG_POSTFIX "d")

include(GNUInstallDirs)
install(TARGETS disassembler
    EXPORT disassembler-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN "*.h")

install(EXPORT disassembler-targets
    NAMESPACE disassembler::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/disassembler
)
