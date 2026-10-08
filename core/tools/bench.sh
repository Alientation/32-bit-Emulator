#!/bin/bash
set -euo pipefail

# Times the emulator on a few programs, so that a change to its speed can be measured.
#
#   tools/bench.sh                 build the release build, run each program 5 times
#   tools/bench.sh -n 10           run each program 10 times
#   tools/bench.sh --perf          also sample each program with perf, and show where the time goes
#   tools/bench.sh --no-build      do not (re)build first
#   tools/bench.sh -b build/debug  use another build directory (a debug build is a lot slower)
#
# The best of the runs is the one to compare, the others only say how much the machine is
# disturbed. Run it from anywhere, it changes to core/.

export LC_ALL=C
cd "$(dirname "${BASH_SOURCE[0]}")/.."

RUNS=5
USE_PERF=false
DO_BUILD=true
BUILD_DIR="build/release"

# The programs: data accesses (membench) and instruction fetch and branches (long_loop).
PROGRAMS=(tools/bench/membench.basm app/programs/src/long_loop.basm)

usage()
{
    echo "Usage: tools/bench.sh [-n RUNS] [--perf] [--no-build] [-b BUILD_DIR]"
}

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
    --perf)
        USE_PERF=true
        shift
        ;;
    --no-build)
        DO_BUILD=false
        shift
        ;;
    -h | --help)
        usage
        exit 0
        ;;
    *)
        echo "Unknown argument '$1'" >&2
        usage >&2
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

# perf: /usr/bin/perf is a wrapper that wants the tools of the running kernel, which a WSL2
# kernel has none of. The tools of any installed kernel work, the system call is the same.
# WSL2 has no hardware counters either, so the clock of the kernel is the event.
find_perf()
{
    local candidate
    for candidate in $(ls -d /usr/lib/linux-tools/*/perf 2>/dev/null | sort -V -r) "$(command -v perf || true)"; do
        if [ -x "$candidate" ] && "$candidate" --version >/dev/null 2>&1; then
            echo "$candidate"
            return 0
        fi
    done
    return 1
}

PERF_BIN=""
if $USE_PERF; then
    PERF_BIN="$(find_perf)" || {
        echo "perf was not found." >&2
        exit 1
    }
fi

echo "build: $BUILD_DIR ($(awk -F= '/^CMAKE_BUILD_TYPE:/ {print $2}' "$BUILD_DIR/CMakeCache.txt")), $RUNS runs each"
printf "%-12s %14s %10s %10s %9s\n" program instructions best\(s\) median\(s\) MIPS

for source in "${PROGRAMS[@]}"; do
    name="$(basename "$source" .basm)"
    if ! "$BASM" -o "$WORK/$name" "$source" -outdir "$WORK" >"$WORK/basm.log" 2>&1; then
        grep -v DBG "$WORK/basm.log" >&2 || true
        echo "Could not assemble $source" >&2
        exit 1
    fi

    times=()
    for ((run = 0; run < RUNS; run++)); do
        start="$EPOCHREALTIME"
        "$EMU" -e "$WORK/$name.bexe" --format plain -o "$WORK/state.txt" >"$WORK/emu.log" 2>&1 || true
        end="$EPOCHREALTIME"
        times+=("$(awk -v s="$start" -v e="$end" 'BEGIN { printf "%.4f", e - s }')")
    done

    if ! grep -q '^status=halted$' "$WORK/state.txt"; then
        echo "$name did not halt:" >&2
        grep -E '^(status|message)=' "$WORK/state.txt" >&2 || true
        exit 1
    fi
    instructions="$(awk -F= '/^instructions=/ {print $2}' "$WORK/state.txt")"

    mapfile -t sorted < <(printf '%s\n' "${times[@]}" | sort -n)
    best="${sorted[0]}"
    median="${sorted[$((${#sorted[@]} / 2))]}"
    mips="$(awk -v n="$instructions" -v t="$best" 'BEGIN { printf "%.1f", n / t / 1e6 }')"
    printf "%-12s %14s %10s %10s %9s\n" "$name" "$instructions" "$best" "$median" "$mips"

    if $USE_PERF; then
        "$PERF_BIN" record -q -e cpu-clock -F 10000 -o "$WORK/perf.data" \
            "$EMU" -e "$WORK/$name.bexe" --format plain -o "$WORK/state.txt" >/dev/null 2>&1 || true
        echo "  where the time goes in $name (symbols, then source files; inlined code counts for the file it is in):"
        "$PERF_BIN" report -i "$WORK/perf.data" --stdio --no-children 2>/dev/null |
            awk '/^ +[0-9.]+%/ { printf "    %6s  %s\n", $1, substr($0, index($0, $5)) }' | head -6
        "$PERF_BIN" report -i "$WORK/perf.data" --stdio --no-children --sort srcline 2>/dev/null |
            awk '/^ +[0-9.]+%/ { pct = $1; sub("%", "", pct); split($2, where, ":"); total[where[1]] += pct }
                 END { for (file in total) printf "    %5.1f%%  %s\n", total[file], file }' |
            sort -rn | head -6
    fi
done
