#!/bin/bash
set -euo pipefail

# Points git at the hooks of the repository (.githooks), so that the pre-push check of the
# formatting runs in this clone. Git does not let a repository turn its hooks on by itself, so
# core/build.sh calls this: whoever builds has the hooks. A core.hooksPath that is set to
# something else is left alone, with a warning.

export LC_ALL=C
cd "$(dirname "${BASH_SOURCE[0]}")/.."

# Not in a git checkout (an exported tarball): nothing to install.
if ! git rev-parse --git-dir > /dev/null 2>&1; then
    exit 0
fi

CURRENT="$(git config --get core.hooksPath || true)"
if [ "$CURRENT" = ".githooks" ]; then
    exit 0
elif [ -n "$CURRENT" ]; then
    echo "core.hooksPath is '$CURRENT', not .githooks: the pre-push formatting check is off" >&2
    exit 0
fi
git config core.hooksPath .githooks
echo "Installed the git hooks (core.hooksPath = .githooks)"
