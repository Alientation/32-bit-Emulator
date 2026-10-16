# Toolchain file of the release build (build.sh passes it): clang 18 with lld, on the standard
# library of GCC 13. The debug and sanitizer builds stay on the default compiler (GCC 13).
#
# Clang 18 makes the emulator 8-26% faster than GCC 13 on most of tools/bench.sh, and GCC 15 and
# 16 are 30% slower on membench and walkbench (docs/internals.md#compilers).
#
# Everything is set through the *_INIT variables, since CMake reads a toolchain file again in the
# projects of its own checks: check_ipo_supported() has to link with the same flags, or it decides
# that LTO is not supported.

find_program(AEMU_CLANG NAMES clang-18 clang REQUIRED)
find_program(AEMU_CLANGXX NAMES clang++-18 clang++ REQUIRED)
find_program(AEMU_LLD NAMES ld.lld-18 ld.lld REQUIRED)
set(CMAKE_C_COMPILER "${AEMU_CLANG}")
set(CMAKE_CXX_COMPILER "${AEMU_CLANGXX}")

# Clang takes the standard library of the newest GCC it finds, which is a GCC that has no
# libstdc++ headers on a plain Ubuntu 24.04 (GCC 14) or one that was installed for something else
# and is slower here (GCC 16). GCC 13's library is the one the benchmarks were measured with.
set(aemu_gcc_flag "")
find_program(AEMU_GCC13 NAMES gcc-13)
if(AEMU_GCC13)
    execute_process(COMMAND "${AEMU_GCC13}" -print-libgcc-file-name
                    OUTPUT_VARIABLE aemu_libgcc OUTPUT_STRIP_TRAILING_WHITESPACE)
    get_filename_component(aemu_gcc_dir "${aemu_libgcc}" DIRECTORY)
    set(aemu_gcc_flag "--gcc-install-dir=${aemu_gcc_dir}")
else()
    message(WARNING "gcc-13 was not found, clang uses the standard library of the newest GCC")
endif()

set(CMAKE_C_FLAGS_INIT "${aemu_gcc_flag}")
set(CMAKE_CXX_FLAGS_INIT "${aemu_gcc_flag}")
foreach(kind EXE SHARED MODULE)
    set(CMAKE_${kind}_LINKER_FLAGS_INIT "--ld-path=${AEMU_LLD}")
endforeach()
