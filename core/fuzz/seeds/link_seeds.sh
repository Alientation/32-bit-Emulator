#!/bin/bash
set -euo pipefail

# Writes the seeds of the link fuzz target (fuzz/link.cpp) to corpus/link: the object file of each
# program in corpus/basm that assembles, alone, and a pair of objects that use each other's symbols.
# An input is two bytes (little endian) with the size of the first object, the first object, then
# the second. Run from anywhere, with a built build/release.

cd "$(dirname "${BASH_SOURCE[0]}")/../.."
BASM="build/release/assembler/basm"
OUT="fuzz/corpus/link"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$OUT"

# The input of one or two objects.
seed() {
    local name="$1" first="$2" second="${3:-}"
    local size
    size=$(stat -c %s "$first")
    printf "\\x$(printf %02x $((size & 255)))\\x$(printf %02x $((size >> 8)))" >"$OUT/$name"
    cat "$first" >>"$OUT/$name"
    [ -n "$second" ] && cat "$second" >>"$OUT/$name"
    return 0
}

for source in fuzz/corpus/basm/seed_*; do
    name="$(basename "$source")"
    # The programs of the preprocessor tests make objects that differ only in a few bytes.
    case "$name" in seed_define_* | seed_macro_* | seed_conditional_*) continue ;; esac
    cp "$source" "$WORK/$name.basm"
    if "$BASM" -c "$WORK/$name.basm" -outdir "$WORK" >/dev/null 2>&1 && [ -f "$WORK/$name.bo" ]; then
        seed "$name" "$WORK/$name.bo"
    fi
done

cat >"$WORK/main.basm" <<'EOF'
.global _start
.global func
.extern counter
.weak maybe
.text
_start:
    bl      func
    adrp    x1, counter
    add     x1, x1, :lo12:counter
    ldr     x2, [x1]
    ldr     x3, =maybe
    hlt
.data
table: .word func, counter, _start
.section "fast", "rx"
fast: ret
EOF
cat >"$WORK/other.basm" <<'EOF'
.global func
.global counter
.text
func:
    add     x0, x0, 1
    ret
.data
counter: .word 7
.bss
.comm   shared, 16, 8
.section "fast", "rx"
    nop
EOF
"$BASM" -c "$WORK/main.basm" "$WORK/other.basm" -outdir "$WORK" >/dev/null
seed seed_pair "$WORK/main.bo" "$WORK/other.bo"
ls "$OUT" | wc -l
