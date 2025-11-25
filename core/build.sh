#!/bin/bash
set -euo pipefail

# Optional: pass "clean" as first argument to remove previous build
CLEAN=${1:-false}

BUILD_DIR="build"
DEBUG_DIR="$BUILD_DIR/debug"
RELEASE_DIR="$BUILD_DIR/release"

if [[ "$CLEAN" == "clean" ]]; then
    echo "Cleaning previous build directories..."
    rm -rf "$BUILD_DIR"
fi

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

# --- Run tests ---
# Run tests for Debug
echo "Running Debug tests..."
ctest --test-dir "$DEBUG_DIR" --progress --output-on-failure

# Run tests for Release
echo "Running Release tests..."
ctest --test-dir "$RELEASE_DIR" --progress --output-on-failure

echo "Build and test completed successfully!"