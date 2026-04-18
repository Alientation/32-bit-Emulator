#!/bin/bash
set -euo pipefail

# Optional arguments:
#   'compile': compile only
#   'test': test only
#   'clean': remove previous build
#   'coverage': generate coverage from tests
ARG1=${1:-false}

BUILD_DIR="build"
DEBUG_DIR="$BUILD_DIR/debug"
RELEASE_DIR="$BUILD_DIR/release"
COVERAGE_DIR="coverage"

DO_COMPILE=true
DO_TEST=true

if [ $# -gt 0 ]; then
    if [[ "$ARG1" == "help" ]]; then
        echo "Optional arguments"
        echo "    help:         Display this message."
        echo "   clean:         Delete build directory and clean build."
        echo "coverage:         Generate code coverage from gcov data. Run after completing tests."
        echo " compile:         Only compile the project. Does not run the unit tests."
        echo "    test:         Only test the project. Must have compiled previously."
        exit
    elif [[ "$ARG1" == "clean" ]]; then
        echo "Cleaning previous build directories..."
        rm -rf "$BUILD_DIR"
    elif [[ "$ARG1" == "coverage" ]]; then
        rm -rf "$COVERAGE_DIR"

        lcov --capture \
            --directory build/debug \
            --output-file coverage.info \
            --rc branch_coverage=1 \
            --ignore-errors mismatch,mismatch \
            --ignore-errors gcov

        lcov --remove coverage.info \
            '*/googletest/*' \
            '*/tests/*' \
            '/usr/include/*' \
            --output-file lcov.info \
            --rc branch_coverage=1 \
            --ignore-errors unused

        genhtml lcov.info \
                --output-directory $COVERAGE_DIR \
                --branch-coverage
        exit
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