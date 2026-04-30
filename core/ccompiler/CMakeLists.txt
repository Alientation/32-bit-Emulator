cmake_minimum_required(VERSION 3.15)
project(ccompiler LANGUAGES C)

add_library(ccompiler STATIC
    src/ccompiler.c
    src/lexer.c
    src/parser.c
    src/codegen.c
    src/stringbuffer.c
    src/vector.c
    src/massert.c
    src/util.c
)

add_executable(ccompiler_exec src/main.c)

target_include_directories(ccompiler
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(ccompiler_exec PRIVATE ccompiler)

if(WIN32)
    target_link_libraries(ccompiler PRIVATE regex)
endif()

foreach(tgt ccompiler ccompiler_exec)
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

add_subdirectory(tests)