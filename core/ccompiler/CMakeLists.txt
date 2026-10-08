add_library(ccompiler STATIC
    src/ccompiler.c
    src/lexer.c
    src/parser.c
    src/codegen.c
    src/stringbuffer.c
    src/massert.c
    src/util.c
)

target_include_directories(ccompiler
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(ccompiler PUBLIC c_std_11)

if(WIN32)
    target_link_libraries(ccompiler PRIVATE regex)
endif()

aemu_target_defaults(ccompiler)

add_executable(ccompiler_exec src/main.c)
target_link_libraries(ccompiler_exec PRIVATE ccompiler)
aemu_target_defaults(ccompiler_exec)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
