# The C compiler. It is plain C that is compiled by the host compiler now and has to compile itself
# later, so it uses nothing but what it supports (docs/ccompiler.md): no VLAs, long, float or double.
add_library(ccompiler STATIC
    src/codegen.c
    src/lex.c
    src/preprocess.c
    src/parse.c
    src/type.c
    src/util.c
)

target_include_directories(ccompiler
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(ccompiler PUBLIC c_std_11)
aemu_target_defaults(ccompiler)
target_compile_options(ccompiler PRIVATE "$<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Werror=vla>")

add_executable(bcc src/main.c)
target_link_libraries(bcc PRIVATE ccompiler)
aemu_target_defaults(bcc)
target_compile_options(bcc PRIVATE "$<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Werror=vla>")

if(BUILD_TESTS)
    # The source of the compiler stays inside the subset it will compile.
    add_test(NAME ccompiler_subset
        COMMAND ${PROJECT_SOURCE_DIR}/tools/check_selfhost_subset.sh
            ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/include)
    set_tests_properties(ccompiler_subset PROPERTIES LABELS ccompiler)

    add_subdirectory(tests)
endif()
