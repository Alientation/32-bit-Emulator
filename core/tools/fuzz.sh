#!/bin/bash
set -euo pipefail

# Fuzzes one target of core/fuzz with libFuzzer (docs/internals.md#fuzzing).
#
#   tools/fuzz.sh TARGET [SECONDS]          build build/fuzz and fuzz TARGET for SECONDS (60)
#   tools/fuzz.sh TARGET SECONDS -- ARGS    with more arguments for libFuzzer (-jobs=8 -workers=8)
#   tools/fuzz.sh TARGET --replay FILE...   run inputs (a crash) once, with the sanitizers' report
#
# The build is clang with the address and undefined behavior sanitizers, at -O1 and with the
# assertions on (a Debug build with the debug and info logging compiled out). The corpus the run
# grows is build/fuzz/corpus/TARGET, started from core/fuzz/corpus/TARGET; a crash is written to
# build/fuzz/artifacts/. Once a crash is fixed, its input goes into core/fuzz/corpus/TARGET, so
# that the replay test (ctest -L fuzz) keeps it fixed. Run it from anywhere, it changes to core/.

export LC_ALL=C
cd "$(dirname "${BASH_SOURCE[0]}")/.."

TARGET="${1:?Usage: tools/fuzz.sh TARGET [SECONDS] [-- LIBFUZZER ARGS] | TARGET --replay FILE...}"
shift
if [ ! -f "fuzz/$TARGET.cpp" ]; then
    echo "There is no fuzz target 'fuzz/$TARGET.cpp'." >&2
    exit 1
fi

BUILD_DIR="build/fuzz"
cmake -S . -B "$BUILD_DIR" -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/clang.cmake \
    -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS_DEBUG="-g -O1" -DCMAKE_CXX_FLAGS_DEBUG="-g -O1" \
    -DAEMU_FUZZ=ON "-DAEMU_SANITIZE=address;undefined" -DAEMU_ENABLE_COVERAGE=OFF \
    -DAEMU_LOG_COMPILE_LEVEL=2 -DBUILD_TESTS=OFF >/dev/null
cmake --build "$BUILD_DIR" --target "${TARGET}_fuzzer"
FUZZER="$BUILD_DIR/fuzz/${TARGET}_fuzzer"

# ASan collects its stacks for the use after return check at every throw, and the emulator throws a
# C++ exception for each fault of a program: with it, two thirds of the time of run_modes went there.
# ./build.sh asan keeps the check.
export ASAN_OPTIONS="detect_stack_use_after_return=0${ASAN_OPTIONS:+:$ASAN_OPTIONS}"

if [ "${1:-}" = "--replay" ]; then
    shift
    exec "$FUZZER" "$@"
fi

SECONDS_TO_RUN="${1:-60}"
[ $# -gt 0 ] && shift
[ "${1:-}" = "--" ] && shift

CORPUS="$BUILD_DIR/corpus/$TARGET"
ARTIFACTS="$BUILD_DIR/artifacts/$TARGET"
mkdir -p "$CORPUS" "$ARTIFACTS"

# -timeout: an input that runs longer is a hang. The first directory is the one that grows, the
# saved inputs are read from the second. It runs in the build directory, where -jobs writes the logs
# of the workers (fuzz-N.log); those of an earlier run are removed first.
CORE="$(pwd)"
cd "$BUILD_DIR"
rm -f fuzz-*.log
"$CORE/$FUZZER" "$CORE/$CORPUS" "$CORE/fuzz/corpus/$TARGET" -max_total_time="$SECONDS_TO_RUN" \
    -timeout=10 -artifact_prefix="$CORE/$ARTIFACTS/" -print_final_stats=1 "$@"
