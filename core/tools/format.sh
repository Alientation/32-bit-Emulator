#!/bin/bash
set -euo pipefail

# Formats the C and C++ sources with clang-format, using the .clang-format at the repo root.
#
#   tools/format.sh                 format every tracked source file in place
#   tools/format.sh --check         change nothing, list the files that are not formatted, exit 1
#                                   if there are any (for CI or a pre-commit check)
#   tools/format.sh [--check] FILE...   only these files (relative to core/)
#
# The files are the ones git tracks with the extension .h, .hpp, .c or .cpp. The inputs of the C
# compiler tests (ccompiler/testfiles) and its runtime (ccompiler/lib) are left alone, they are
# data and not code that follows the style. Set CLANG_FORMAT to use another binary, e.g.
# CLANG_FORMAT=clang-format-18 tools/format.sh.

export LC_ALL=C
cd "$(dirname "${BASH_SOURCE[0]}")/.."

CLANG_FORMAT="${CLANG_FORMAT:-clang-format}"
CHECK=0
if [ "${1:-}" = "--check" ]; then
    CHECK=1
    shift
fi

if ! command -v "$CLANG_FORMAT" > /dev/null; then
    echo "No '$CLANG_FORMAT' found; install clang-format or set CLANG_FORMAT" >&2
    exit 1
fi

FILES=()
if [ "$#" -gt 0 ]; then
    for f in "$@"; do
        if [ ! -f "$f" ]; then
            echo "No file '$f' (relative to core/)" >&2
            exit 1
        fi
        FILES+=("$f")
    done
else
    while IFS= read -r -d '' f; do
        FILES+=("$f")
    done < <(git ls-files -z -- '*.h' '*.hpp' '*.c' '*.cpp' ':!:ccompiler/testfiles' ':!:ccompiler/lib')
fi

if [ "${#FILES[@]}" -eq 0 ]; then
    echo "No source files found" >&2
    exit 1
fi

if [ "$CHECK" -eq 1 ]; then
    BAD=()
    for f in "${FILES[@]}"; do
        if ! "$CLANG_FORMAT" "$f" | cmp -s "$f" -; then
            BAD+=("$f")
        fi
    done
    if [ "${#BAD[@]}" -gt 0 ]; then
        echo "Not formatted (${#BAD[@]} of ${#FILES[@]} files), run tools/format.sh:" >&2
        printf '  %s\n' "${BAD[@]}" >&2
        exit 1
    fi
    echo "All ${#FILES[@]} files are formatted"
else
    "$CLANG_FORMAT" -i -- "${FILES[@]}"
    echo "Formatted ${#FILES[@]} files"
fi
