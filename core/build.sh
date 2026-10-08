#!/bin/bash
set -euo pipefail

# Optional arguments:
#   'compile': compile only
#   'test': test only
#   'clean': remove previous build
#   'coverage': generate coverage from tests
#   'asan': build and test with the address, leak and undefined behavior sanitizers (build/asan)
#   'ubsan': build and test with the undefined behavior sanitizer alone (build/ubsan), which is
#            a lot faster than 'asan'
ARG1=${1:-false}

BUILD_DIR="build"
DEBUG_DIR="$BUILD_DIR/debug"
RELEASE_DIR="$BUILD_DIR/release"
COVERAGE_DIR="coverage"

DO_COMPILE=true
DO_TEST=true

# A sanitizer build replaces the debug and release builds. It has a directory of its own, since
# the whole tree has to be built with the same flags.
DO_SANITIZE=false
SANITIZE_DIR=""
SANITIZE_LIST=""

if [ $# -gt 0 ]; then
    if [[ "$ARG1" == "help" ]]; then
        echo "Optional arguments"
        echo "    help:         Display this message."
        echo "   clean:         Delete build directory and clean build."
        echo "coverage:         Generate code coverage from gcov data. Run after completing tests."
        echo " compile:         Only compile the project. Does not run the unit tests."
        echo "    test:         Only test the project. Must have compiled previously."
        echo "    asan:         Build (build/asan) and test with the address, leak and undefined"
        echo "                  behavior sanitizers instead of the debug and release builds."
        echo "   ubsan:         Same with the undefined behavior sanitizer alone (build/ubsan)."
        exit
    elif [[ "$ARG1" == "clean" ]]; then
        echo "Cleaning previous build directories..."
        rm -rf "$BUILD_DIR"
    elif [[ "$ARG1" == "coverage" ]]; then
        rm -rf "$COVERAGE_DIR"

        # Capture and exclude tests/external code simultaneously
        # (This completely eliminates the tst_test.cpp / smull_test.cpp warnings)
        lcov --capture                  \
            --directory build/debug     \
            --base-directory .          \
            --output-file lcov.info     \
            --rc branch_coverage=1      \
            --ignore-errors mismatch    \
            --ignore-errors gcov        \
            --no-external               \
            --exclude '*/tests/*'       \
            --exclude '*/cxxopts/*'     \
            --exclude '*/googletest/*'  \
            --exclude '*/integration_tests/*'

        # Generate the HTML report directly from lcov.info
        genhtml lcov.info                       \
            --output-directory "$COVERAGE_DIR"  \
            --branch-coverage                   \
            --function-coverage
        exit
    elif [[ "$ARG1" == "asan" || "$ARG1" == "ubsan" ]]; then
        echo "Building and testing with sanitizers ($ARG1)..."
        DO_COMPILE=false
        DO_TEST=false
        DO_SANITIZE=true
        SANITIZE_DIR="$BUILD_DIR/$ARG1"
        if [[ "$ARG1" == "asan" ]]; then
            SANITIZE_LIST="address;undefined"
        else
            SANITIZE_LIST="undefined"
        fi
    elif [[ "$ARG1" == "compile" ]]; then
        echo "Only compiling..."
        DO_COMPILE=true
        DO_TEST=false
    elif [[ "$ARG1" == "test" ]]; then
        echo "Only testing..."
        DO_COMPILE=false
        DO_TEST=true
    else
        echo "Unknown argument '$ARG1'"
        exit 1
    fi
fi


if $DO_COMPILE; then
    # Create build directories
    mkdir -p "$DEBUG_DIR" "$RELEASE_DIR"

    # --- Configure builds ---
    echo "Configuring Debug build..."
    cmake -S . -B "$DEBUG_DIR" -G "Ninja" -DCMAKE_BUILD_TYPE=Debug

    echo "Configuring Release build..."
    cmake -S . -B "$RELEASE_DIR" -G "Ninja" -DCMAKE_BUILD_TYPE=RelWithDebInfo

    # --- Build ---
    echo "Building Debug..."
    cmake --build "$DEBUG_DIR"

    echo "Building Release..."
    cmake --build "$RELEASE_DIR"
fi

if $DO_TEST; then
    # Clear stale coverage data
    echo "Clearing stale coverage data..."
    find "$DEBUG_DIR" -name "*.gcda" -delete

    # --- Run tests ---
    # Run tests for Debug
    echo "Running Debug tests..."
    ctest --test-dir "$DEBUG_DIR" --progress --output-on-failure

    # Run tests for Release
    echo "Running Release tests..."
    ctest --test-dir "$RELEASE_DIR" --progress --output-on-failure
fi

if $DO_SANITIZE; then
    # Debug, so that the reports have the source lines. No coverage, it only slows the run down.
    echo "Configuring sanitizer build ($SANITIZE_LIST)..."
    cmake -S . -B "$SANITIZE_DIR" -G "Ninja" -DCMAKE_BUILD_TYPE=Debug -DAEMU_ENABLE_COVERAGE=OFF \
        "-DAEMU_SANITIZE=$SANITIZE_LIST"

    echo "Building with sanitizers..."
    cmake --build "$SANITIZE_DIR"

    # The tests start basm and emu32, which are built with the sanitizers too and read these.
    echo "Running sanitizer tests..."
    ASAN_OPTIONS="detect_leaks=1:strict_string_checks=1:detect_stack_use_after_return=1" \
    UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1" \
        ctest --test-dir "$SANITIZE_DIR" --progress --output-on-failure
fi