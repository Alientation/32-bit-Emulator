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
add_library(aemu::assembler ALIAS assembler)

target_include_directories(assembler
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(assembler PUBLIC cxx_std_20)
target_link_libraries(assembler PUBLIC aemu::util aemu::emulator32bit)
aemu_target_defaults(assembler)

set_target_properties(assembler PROPERTIES DEBUG_POSTFIX "d")

add_executable(basm src/main.cpp)
target_link_libraries(basm PRIVATE aemu::assembler)
aemu_target_defaults(basm)

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

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
