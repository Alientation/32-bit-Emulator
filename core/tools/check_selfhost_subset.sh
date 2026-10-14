#!/bin/bash
set -euo pipefail

# Checks that the C compiler's source only uses what the compiler is meant to compile itself with
# (docs/ccompiler.md): no `long`, `float` or `double`. Comments and the text of string literals are
# not looked at. (A variable length array is an error of the compiler build, -Werror=vla.)
#
#   tools/check_selfhost_subset.sh DIR...

export LC_ALL=C

if [ "$#" -eq 0 ]; then
    echo "usage: check_selfhost_subset.sh DIR..." >&2
    exit 2
fi

BAD=0
while IFS= read -r -d '' f; do
    HITS=$(sed -E 's://.*$::; s:"([^"\\]|\\.)*"::g' "$f" \
        | grep -n -E '\b(long|float|double|_Complex)\b' || true)
    if [ -n "$HITS" ]; then
        echo "$f uses a type the compiler cannot compile itself with yet:" >&2
        echo "$HITS" >&2
        BAD=1
    fi
done < <(find "$@" \( -name '*.c' -o -name '*.h' \) -print0)

exit "$BAD"
