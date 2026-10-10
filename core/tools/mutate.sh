#!/bin/bash
set -euo pipefail

# Checks that the tests notice when a line of the code is gone: a hand made mutation test. Run it
# on the lines of the code that the tests are meant to guard (a cache invalidation, a flag that
# makes the pc skip a step, ...), above all in code that was written together with its tests.
#
#   tools/mutate.sh FILE LINE [TEXT]    replace line LINE of FILE with a comment, build the
#                                       debug build, run the tests, put the file back
#
#   tools/mutate.sh emulator32bit/src/virtual_memory.cpp 133 drop_fetch_cache
#
# TEXT, if given, has to be in that line, so a line that moved is not mutated by mistake. The file
# is saved to a copy and restored from it (not with git), so uncommitted changes are kept; if the
# script is killed, the copy is FILE.orig. The result is `killed` when a test fails (good), and
# `SURVIVED` when none does: either the line is redundant (another line does the same, then say
# so with a comment in the code) or a test is missing. Commented out lines are only a mutation
# when they are a whole statement.

export LC_ALL=C
cd "$(dirname "${BASH_SOURCE[0]}")/.."

FILE="${1:?usage: tools/mutate.sh FILE LINE [TEXT]}"
LINE="${2:?usage: tools/mutate.sh FILE LINE [TEXT]}"
TEXT="${3:-}"
BUILD_DIR="build/debug"

if [ ! -f "$FILE" ]; then
    echo "No file '$FILE' (relative to core/)" >&2
    exit 1
fi
CURRENT="$(sed -n "${LINE}p" "$FILE")"
if [ -z "$CURRENT" ]; then
    echo "Line $LINE of $FILE is empty or past the end" >&2
    exit 1
fi
if [ -n "$TEXT" ] && [[ "$CURRENT" != *"$TEXT"* ]]; then
    echo "Line $LINE of $FILE is '$CURRENT', which does not contain '$TEXT'" >&2
    exit 1
fi

cp "$FILE" "$FILE.orig"
trap 'mv -f "$FILE.orig" "$FILE"; touch "$FILE"' EXIT

sed -i "${LINE}s|.*|    // MUTATED: ${CURRENT//|/\\|}|" "$FILE"
echo "mutated $FILE:$LINE  $CURRENT"

if ! cmake --build "$BUILD_DIR" >/dev/null 2>&1; then
    echo "The mutant does not build, pick another line (a statement that others use)" >&2
    exit 2
fi

if OUT="$(ctest --test-dir "$BUILD_DIR" -j8 2>&1)"; then
    echo "SURVIVED: all the tests pass without this line"
    STATUS=1
else
    echo "killed by:"
    echo "$OUT" | grep -E '\*\*\*Failed|\(Failed\)|Failed  ' | sed -E 's/.*Test +#[0-9]+: +//; s/ \.+.*//' | head -8 | sed 's/^/  /'
    STATUS=0
fi
exit $STATUS
