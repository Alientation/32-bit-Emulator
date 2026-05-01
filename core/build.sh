#!/bin/bash
set -euo pipefail

BUILD_DIR="build"
DEBUG_DIR="$BUILD_DIR/debug"
RELEASE_DIR="$BUILD_DIR/release"
COVERAGE_DIR="coverage"

# Default state: if no args are passed, do everything
DO_CLEAN=false
DO_COMPILE=false
DO_TEST=false
DO_VALGRIND=false
DO_COVERAGE=false

# If no arguments provided, default to compile and test
if [ $# -eq 0 ]; then
    DO_COMPILE=true
    DO_TEST=true
else
    # Parse all arguments
    for arg in "$@"; do
        case $arg in
            help)
                echo "Usage: ./build.sh [options]"
                echo "Options:"
                echo "  clean    : Delete build directory"
                echo "  compile  : Run cmake config and build"
                echo "  test     : Run ctest"
                echo "  valgrind : Run ctest with memcheck (implies 'test')"
                echo "  coverage : Generate lcov report (implies 'test')"
                exit 0
                ;;
            clean)    DO_CLEAN=true ;;
            compile)  DO_COMPILE=true ;;
            test)     DO_TEST=true ;;
            valgrind) DO_VALGRIND=true; DO_TEST=true ;;
            coverage) DO_COVERAGE=true; DO_TEST=true ;;
            *) echo "Unknown argument: $arg"; exit 1 ;;
        esac
    done
fi

if $DO_CLEAN; then
    echo "Cleaning previous build..."
    rm -rf "$BUILD_DIR" "$COVERAGE_DIR"
fi

if $DO_COMPILE; then
    mkdir -p "$DEBUG_DIR" "$RELEASE_DIR"
    echo "Configuring and Building..."
    cmake -S . -B "$DEBUG_DIR" -G "Ninja" -DCMAKE_BUILD_TYPE=Debug
    cmake --build "$DEBUG_DIR"
    cmake -S . -B "$RELEASE_DIR" -G "Ninja" -DCMAKE_BUILD_TYPE=RelWithDebInfo
    cmake --build "$RELEASE_DIR"
fi

if $DO_TEST; then
    echo "Clearing stale coverage data..."
    find "$DEBUG_DIR" -name "*.gcda" -delete 2>/dev/null || true

    if $DO_VALGRIND; then
        echo "Running Debug tests with Valgrind..."
        # Using -D to inject the path in case DartConfiguration.tcl is missing/empty
        ctest --test-dir "$DEBUG_DIR" -T memcheck --progress --output-on-failure \
              -D MemoryCheckCommand="$(which valgrind)"
    else
        echo "Running Debug tests..."
        ctest --test-dir "$DEBUG_DIR" --progress --output-on-failure
    fi

    echo "Running Release tests..."
    ctest --test-dir "$RELEASE_DIR" --progress --output-on-failure
fi

if $DO_COVERAGE; then
    echo "Generating coverage report..."

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
fi