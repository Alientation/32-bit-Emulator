#!/bin/bash
set -euo pipefail

# Writes the seeds of the library fuzz target (fuzz/library.cpp) to corpus/library: a program and
# a library of four members, of which it needs three (one only through another member), and a
# program that needs no member of a library of two. An input is two bytes (little endian) with the
# size of the program's object, the object, then the library. Run from anywhere, with a built
# build/release.

cd "$(dirname "${BASH_SOURCE[0]}")/../.."
BASM="build/release/assembler/basm"
OUT="fuzz/corpus/library"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$OUT"

# The input of an object and a library.
seed() {
    local name="$1" object="$2" library="$3"
    local size
    size=$(stat -c %s "$object")
    printf "\\x$(printf %02x $((size & 255)))\\x$(printf %02x $((size >> 8)))" >"$OUT/$name"
    cat "$object" "$library" >>"$OUT/$name"
}

cat >"$WORK/main.basm" <<'BASM'
.global _start
.extern func
.extern counter
.extern helper
.weak maybe
.text
_start:
    bl      func
    bl      helper
    adrp    x1, counter
    add     x1, x1, :lo12:counter
    ldr     x2, [x1]
    ldr     x3, =maybe
    hlt
BASM
cat >"$WORK/func.basm" <<'BASM'
.global func
.global counter
.text
func:
    add     x0, x0, 1
    ret
.data
counter: .word 7
BASM
cat >"$WORK/helper.basm" <<'BASM'
.global helper
.extern deep
.text
helper:
    mov     x9, x29
    bl      deep
    mov     x29, x9
    ret
.section "fast", "rx"
    nop
BASM
cat >"$WORK/deep.basm" <<'BASM'
.global deep
.text
deep:
    add     x0, x0, 2
    ret
.bss
.comm   shared, 16, 8
BASM
cat >"$WORK/unused.basm" <<'BASM'
.global maybe
.global func
.text
maybe:
func:
    hlt
BASM
cat >"$WORK/alone.basm" <<'BASM'
.global _start
.text
_start:
    mov     x0, 1
    hlt
BASM
for name in main alone; do
    "$BASM" -c "$WORK/$name.basm" -outdir "$WORK" >/dev/null
done
"$BASM" -ar "$WORK/func.basm" "$WORK/helper.basm" "$WORK/deep.basm" "$WORK/unused.basm" -outdir "$WORK" -o "$WORK/lib" >/dev/null
"$BASM" -ar "$WORK/func.basm" "$WORK/deep.basm" -outdir "$WORK" -o "$WORK/small" >/dev/null
seed seed_three_of_four_members "$WORK/main.bo" "$WORK/lib.ba"
seed seed_no_member_needed "$WORK/alone.bo" "$WORK/small.ba"
ls "$OUT" | wc -l
