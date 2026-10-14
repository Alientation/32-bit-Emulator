#!/bin/bash
set -euo pipefail

# Checks that the C compiler's source only uses what the compiler is meant to compile itself with
# (docs/ccompiler.md): `long long` and `double`, but no plain `long` (64 bit on the host, 32 on the
# target), `long double` (wider on the host), `float` or `_Complex`. Comments, `#include` lines and
# the text of string literals are not looked at. (A variable length array is an error of the
# compiler build, -Werror=vla.)
#
#   tools/check_selfhost_subset.sh DIR...

export LC_ALL=C

if [ "$#" -eq 0 ]; then
    echo "usage: check_selfhost_subset.sh DIR..." >&2
    exit 2
fi

BAD=0
while IFS= read -r -d '' f; do
    HITS=$(sed -E 's:^#[[:space:]]*include.*$::; s://.*$::; s:"([^"\\]|\\.)*"::g' "$f" \
        | sed -E 's:\blong[[:space:]]+long\b:LLONG:g' \
        | grep -n -E '\b(long|float|_Complex)\b' || true)
    if [ -n "$HITS" ]; then
        echo "$f uses a type outside the subset the compiler compiles itself with:" >&2
        echo "$HITS" >&2
        BAD=1
    fi
done < <(find "$@" \( -name '*.c' -o -name '*.h' \) -print0)

exit "$BAD"
