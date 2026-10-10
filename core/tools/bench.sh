#!/bin/bash
set -euo pipefail

# Times the emulator on a few programs, so that a change to its speed can be measured.
#
#   tools/bench.sh                 build the release build, run each program 5 times
#   tools/bench.sh -n 10           run each program 10 times
#   tools/bench.sh --perf          also sample each program with perf, and show where the time goes
#   tools/bench.sh --no-build      do not (re)build first
#   tools/bench.sh --only REGEX    run only the programs whose label matches (--only 'callbench|crc')
#   tools/bench.sh -b build/debug  use another build directory (a debug build is a lot slower)
#   tools/bench.sh --baseline EMU  compare with another emu32 (one built from another commit):
#                                  the two run alternately, so that what the machine is doing
#                                  affects both the same way
#
# The best of the runs is the one to compare, the others only say how much the machine is
# disturbed. A single run of the same binary can differ by 5% or more, so decide on a change with
# --baseline and a good number of runs (-n 15), not on two separate runs. Run it from anywhere, it
# changes to core/.

export LC_ALL=C
cd "$(dirname "${BASH_SOURCE[0]}")/.."

RUNS=5
USE_PERF=false
DO_BUILD=true
BUILD_DIR="build/release"
BASELINE=""
ONLY=""

# What is run. A line is `label|source|emulator arguments|expected state`:
#
#   label       the name in the table; a program that runs more than once with other arguments has
#               a label of its own for each (callbench+history)
#   source      the .basm file, linked with the .ld next to it if there is one
#   arguments   more arguments for emu32 (the words are split at spaces), may be empty
#   expected    `key=value` pairs separated by commas, that the final state (--format plain) must
#               have: the result of the program, so that a change that makes it faster by making
#               it wrong is not taken for an improvement. `status=halted` is implied when there is
#               no `status`. The values were computed independently of the emulator (zlib's crc32,
#               a reference in Python, fib(30) = 832040, ...), and the comment of each program says
#               what its result is.
#
# What the programs measure:
#   data accesses: membench (one address), walkbench (the same with page tables), memcpybench
#       (sequential over 12 pages, with unaligned accesses), crcbench (a table indexed by the data),
#       sortbench (indexed accesses and branches that depend on the data), sievebench (a byte
#       store at a stride), matmulbench (two streams, one by rows and one by columns)
#   instruction fetch and dispatch: long_loop (a tight loop), callbench (calls and the stack),
#       mixbench (instructions in a random order), jumpbench (indirect jumps between 16 pages),
#       fpbench (floating point, where the host FPU is asked for every result)
#   the memory system: swapbench (page faults, whose MIPS mean little: it runs few instructions and
#       many page faults, compare its time), tlbbench (TLB misses that are not faults)
#   the rest of the machine: excbench (system calls, data aborts and timer interrupts; compare the
#       time), atomicbench (the atomic instructions), devbench (console, DMA and the data register
#       of the block device, wfi)
#   the per instruction hooks of the debugger, which run() decides on once at its start: each
#       `+x` line is another program with an option that looks at every instruction (history,
#       a breakpoint that is never reached, a watched register, a watched address, a trace to
#       /dev/null cut at a million instructions) and compares with the line without it
BENCHES=(
    "membench|tools/bench/membench.basm||x2=0x003d0900"
    "walkbench|tools/bench/walkbench.basm||x2=0x003d0900"
    "long_loop|app/programs/src/long_loop.basm||x0=0x00000000"
    "callbench|tools/bench/callbench.basm||x0=0x000cb228"
    "sortbench|tools/bench/sortbench.basm||x0=0x00000000,x15=0x406cd800"
    "mixbench|tools/bench/mixbench.basm||x2=0xf9849dac,x3=0x29e3eabb"
    "swapbench|tools/bench/swapbench.basm||x5=0xb720dba0"
    "tlbbench|tools/bench/tlbbench.basm||x2=0xbccfe4a0"
    "fpbench|tools/bench/fpbench.basm||x3=0x3f800000,x9=0x3f800000,x15=0x3ff00000,x17=0x3ff00000"
    "memcpybench|tools/bench/memcpybench.basm||x20=0x4e2101f2"
    "crcbench|tools/bench/crcbench.basm||x0=0xa57970a5"
    "sievebench|tools/bench/sievebench.basm||x20=0x0002fea4"
    "matmulbench|tools/bench/matmulbench.basm||x20=0x1e7b5e6d"
    "jumpbench|tools/bench/jumpbench.basm||x2=0x55a5033f"
    "excbench|tools/bench/excbench.basm||x7=0x000186a0,x10=0x00061a80,x13=0x000186a0"
    "atomicbench|tools/bench/atomicbench.basm||x20=0x00895440,x21=0x00000040,x22=0x0000ff0f,x23=0x00000000"
    "devbench|tools/bench/devbench.basm|--block-sectors 8|x24=0x00186a00,x25=0xe03dd3a0,x26=0x17b8da6c"
    "callbench+history|tools/bench/callbench.basm|--history 1000|x0=0x000cb228"
    "callbench+break|tools/bench/callbench.basm|--break 0x7000|x0=0x000cb228"
    "mixbench+watchreg|tools/bench/mixbench.basm|--watch-reg x20|x2=0xf9849dac,x3=0x29e3eabb"
    "membench+watch|tools/bench/membench.basm|--watch 0x1020:4:rw|x2=0x003d0900"
    "callbench+trace|tools/bench/callbench.basm|--trace /dev/null -l 1000000|status=limit,instructions=1000000"
)

usage()
{
    echo "Usage: tools/bench.sh [-n RUNS] [--only REGEX] [--perf] [--no-build] [-b BUILD_DIR] [--baseline EMU32]"
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
    --baseline)
        BASELINE="${2:?--baseline needs the emu32 to compare with}"
        shift 2
        ;;
    --only)
        ONLY="${2:?--only needs a regular expression}"
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

if [ -n "$BASELINE" ] && [ ! -x "$BASELINE" ]; then
    echo "The baseline '$BASELINE' is not an executable." >&2
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

# Seconds that the emulator $1 takes to run the executable $2, with the arguments of RUN_ARGS. What
# it prints goes nowhere: the state file has the result, and a log in a file would put the speed of
# the host's file system into devbench, whose console flushes every byte.
time_once()
{
    local start end
    start="$EPOCHREALTIME"
    "$1" -e "$2" ${RUN_ARGS[@]+"${RUN_ARGS[@]}"} --format plain -o "$WORK/state.txt" \
        >/dev/null 2>&1 || true
    end="$EPOCHREALTIME"
    awk -v s="$start" -v e="$end" 'BEGIN { printf "%.4f", e - s }'
}

# The best and the median of the times that are given.
best_and_median()
{
    local sorted
    mapfile -t sorted < <(printf '%s\n' "$@" | sort -n)
    echo "${sorted[0]} ${sorted[$((${#sorted[@]} / 2))]}"
}

# Checks the state of the last run against the expected pairs $1 (`key=value,key=value`), which
# have status=halted added if they do not say how the run ends. Prints what differs and returns 1.
check_state()
{
    local label="$1" expected="$2" pair key ok=true
    case ",$expected," in
    *,status=*) ;;
    *) expected="status=halted${expected:+,$expected}" ;;
    esac
    local IFS=,
    for pair in $expected; do
        key="${pair%%=*}"
        if ! grep -qx -- "$pair" "$WORK/state.txt"; then
            echo "$label: expected $pair, got $(grep -m1 "^$key=" "$WORK/state.txt" || echo "no $key")" >&2
            ok=false
        fi
    done
    if ! $ok; then
        grep -E '^(status|message)=' "$WORK/state.txt" >&2 || true
        return 1
    fi
}

if [ -n "$BASELINE" ]; then
    echo "build: $BUILD_DIR ($(awk -F= '/^CMAKE_BUILD_TYPE:/ {print $2}' "$BUILD_DIR/CMakeCache.txt")), $RUNS runs each, alternately with $BASELINE"
    printf "%-18s %14s %10s %10s %8s %10s %10s\n" program instructions base\(s\) new\(s\) change base-med new-med
else
    echo "build: $BUILD_DIR ($(awk -F= '/^CMAKE_BUILD_TYPE:/ {print $2}' "$BUILD_DIR/CMakeCache.txt")), $RUNS runs each"
    printf "%-18s %14s %10s %10s %9s\n" program instructions best\(s\) median\(s\) MIPS
fi

ran=0
for spec in "${BENCHES[@]}"; do
    IFS='|' read -r label source emu_args expected <<<"$spec"
    if [ -n "$ONLY" ] && ! [[ "$label" =~ $ONLY ]]; then
        continue
    fi
    ran=$((ran + 1))
    name="$(basename "$source" .basm)"
    read -r -a RUN_ARGS <<<"$emu_args"

    # A program that is run with other arguments is only assembled once.
    if [ ! -f "$WORK/$name.bexe" ]; then
        link_args=()
        if [ -f "${source%.basm}.ld" ]; then
            link_args=(-ld "${source%.basm}.ld")
        fi
        if ! "$BASM" -o "$WORK/$name" "$source" ${link_args[@]+"${link_args[@]}"} -outdir "$WORK" \
            >"$WORK/basm.log" 2>&1; then
            grep -v DBG "$WORK/basm.log" >&2 || true
            echo "Could not assemble $source" >&2
            exit 1
        fi
    fi

    times=()
    base_times=()
    for ((run = 0; run < RUNS; run++)); do
        if [ -n "$BASELINE" ]; then
            base_times+=("$(time_once "$BASELINE" "$WORK/$name.bexe")")
        fi
        times+=("$(time_once "$EMU" "$WORK/$name.bexe")")
    done

    # The state of the last run, which is the emulator that is measured.
    check_state "$label" "$expected" || exit 1
    instructions="$(awk -F= '/^instructions=/ {print $2}' "$WORK/state.txt")"

    read -r best median <<<"$(best_and_median "${times[@]}")"
    if [ -n "$BASELINE" ]; then
        read -r base_best base_median <<<"$(best_and_median "${base_times[@]}")"
        change="$(awk -v n="$best" -v b="$base_best" 'BEGIN { printf "%+.1f%%", (n / b - 1) * 100 }')"
        printf "%-18s %14s %10s %10s %8s %10s %10s\n" "$label" "$instructions" "$base_best" "$best" \
            "$change" "$base_median" "$median"
    else
        mips="$(awk -v n="$instructions" -v t="$best" 'BEGIN { printf "%.1f", n / t / 1e6 }')"
        printf "%-18s %14s %10s %10s %9s\n" "$label" "$instructions" "$best" "$median" "$mips"
    fi

    if $USE_PERF; then
        "$PERF_BIN" record -q -e cpu-clock -F 10000 -o "$WORK/perf.data" \
            "$EMU" -e "$WORK/$name.bexe" ${RUN_ARGS[@]+"${RUN_ARGS[@]}"} --format plain \
            -o "$WORK/state.txt" >/dev/null 2>&1 || true
        echo "  where the time goes in $label (symbols, then source files; inlined code counts for the file it is in):"
        "$PERF_BIN" report -i "$WORK/perf.data" --stdio --no-children 2>/dev/null |
            awk '/^ +[0-9.]+%/ { printf "    %6s  %s\n", $1, substr($0, index($0, $5)) }' | head -6
        "$PERF_BIN" report -i "$WORK/perf.data" --stdio --no-children --sort srcline 2>/dev/null |
            awk '/^ +[0-9.]+%/ { pct = $1; sub("%", "", pct); split($2, where, ":"); total[where[1]] += pct }
                 END { for (file in total) printf "    %5.1f%%  %s\n", total[file], file }' |
            sort -rn | head -6
    fi
done

if [ "$ran" -eq 0 ]; then
    echo "No program matches '$ONLY'." >&2
    exit 1
fi
