#!/bin/bash
set -euo pipefail

# Times basm (preprocess, assemble, link) and the loader of emu32 on generated programs, and checks
# that the time grows about linearly with the size of the input.
#
#   tools/bench_toolchain.sh              build the release build, run each size 3 times
#   tools/bench_toolchain.sh -n 5         run each size 5 times
#   tools/bench_toolchain.sh --no-build   do not (re)build first
#   tools/bench_toolchain.sh -b DIR       use another build directory
#
# The programs come from tools/gen_basm.py: one file with 8000, 16000 and 32000 functions (a
# single file is where a quadratic step in the assembler shows), and 20 files of 1000 functions
# (the linker and the relocations). The time of the best run is printed with the lines per second.
# Doubling a file should about double the time: a ratio over 3 fails the script (the 32000 / 16000
# ratio of a quadratic step is 4). The loader is timed on the biggest executable with -l 1.
# A time is the best of the runs, because a single run is disturbed by the machine.

export LC_ALL=C
cd "$(dirname "${BASH_SOURCE[0]}")/.."

RUNS=3
DO_BUILD=true
BUILD_DIR="build/release"
MAX_RATIO=3

while [ $# -gt 0 ]; do
    case "$1" in
    -n)
        RUNS="${2:?-n needs a number}"
        shift 2
        ;;
    -b | --build-dir)
        BUILD_DIR="${2:?-b needs a directory}"
        shift 2
        ;;
    --no-build)
        DO_BUILD=false
        shift
        ;;
    -h | --help)
        echo "Usage: tools/bench_toolchain.sh [-n RUNS] [--no-build] [-b BUILD_DIR]"
        exit 0
        ;;
    *)
        echo "Unknown argument '$1'" >&2
        exit 1
        ;;
    esac
done

if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    echo "No build in '$BUILD_DIR'. Run ./build.sh compile first." >&2
    exit 1
fi
if $DO_BUILD; then
    cmake --build "$BUILD_DIR" --target basm emu32 >/dev/null
fi

BASM="$BUILD_DIR/assembler/basm"
EMU="$BUILD_DIR/emulator32bit/emu32"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

# best_time COMMAND...: the shortest wall time in seconds of RUNS runs.
best_time()
{
    local best=""
    for ((r = 0; r < RUNS; r++)); do
        local start end elapsed
        start=$(date +%s.%N)
        "$@" >/dev/null 2>&1
        end=$(date +%s.%N)
        elapsed=$(echo "$end - $start" | bc -l)
        if [ -z "$best" ] || [ "$(echo "$elapsed < $best" | bc -l)" = 1 ]; then best=$elapsed; fi
    done
    echo "$best"
}

# build NAME FILES FUNCTIONS: generates the program, prints a row and keeps the time in TIMES[NAME].
declare -A TIMES
build()
{
    local name=$1 files=$2 per_file=$3
    python3 tools/gen_basm.py "$WORK/$name" "$files" "$per_file"
    mkdir -p "$WORK/$name/out"
    local lines
    lines=$(cat "$WORK/$name"/*.basm | wc -l)
    TIMES[$name]=$(best_time "$BASM" -o "$WORK/$name/out/prog" -outdir "$WORK/$name/out" "$WORK/$name"/*.basm)
    printf "%-14s %2d file(s) %8d lines  %7.3f s  %9.0f lines/s\n" "$name" "$files" "$lines" \
        "${TIMES[$name]}" "$(echo "$lines / ${TIMES[$name]}" | bc -l)"
}

build one8000 1 8000
build one16000 1 16000
build one32000 1 32000
build twenty_files 20 1000


echo "loader: $(printf '%.3f' "$(best_time "$EMU" -e "$WORK/twenty_files/out/prog.bexe" -l 1 --format plain)") s" \
    "for $(stat -c %s "$WORK/twenty_files/out/prog.bexe") bytes"

RATIO=$(echo "${TIMES[one32000]} / ${TIMES[one16000]}" | bc -l)
printf "32000 / 16000 functions: x%.2f (linear is 2, quadratic 4)\n" "$RATIO"
if [ "$(echo "$RATIO > $MAX_RATIO" | bc -l)" = 1 ]; then
    echo "FAIL: the time of basm grows faster than linearly with the size of one file" >&2
    exit 1
fi
