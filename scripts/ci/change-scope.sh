#!/usr/bin/env bash
# Decide whether a pull request needs the builds and the test suite.
#
# A pull request that changes only documentation skips the heavy work: the
# build jobs still start and report, so every required check passes, but they
# do nothing. Everything else, and every push and manual run, builds.
#
# Documentation means:
#   - any *.md file;
#   - anything under website/;
#   - anything else under docs/, unless its folder is named in src/, tests/,
#     cmake/, resources/ or CMakeLists.txt. tst_app_icon compares the icons
#     in a docs/architecture design folder with the app's own, so a change
#     there has to run the tests.
# No build step or test reads a .md file (checked 2026-10-16: no CMake file,
# resource list or test names one), so .md files need no such check.
#
# Run from the root of a checkout of the pull request's merge commit with at
# least two commits of history (actions/checkout fetch-depth: 2). Writes
# heavy=true or heavy=false to $GITHUB_OUTPUT, or to stdout without it, and
# says why. Anything unexpected (no parent, a failed diff, an empty list)
# means heavy=true: a wrong "true" costs a build, a wrong "false" lets an
# untested change through.
#
#   scripts/ci/change-scope.sh <github.event_name>
set -uo pipefail

out="${GITHUB_OUTPUT:-/dev/stdout}"

decide() {
    echo "heavy=$1" >> "$out"
    echo "$2"
    exit 0
}

if [ "${1:-}" != "pull_request" ]; then
    decide true "Not a pull request: every build runs."
fi

# The merge commit's first parent is the base branch, so this is exactly
# what the pull request changes.
if ! files="$(git diff --name-only HEAD^1 HEAD 2>/dev/null)"; then
    decide true "Could not list the pull request's files: every build runs."
fi
if [ -z "$files" ]; then
    decide true "The pull request changes no files: every build runs."
fi

while IFS= read -r file; do
    case "$file" in
        website/*)
            ;;
        *.md)
            ;;
        docs/*)
            dir="$(dirname "$file")/"
            if git grep -q -F -e "$dir" -- src tests cmake resources CMakeLists.txt; then
                decide true "$file: the build or a test reads $dir, so every build runs."
            fi
            ;;
        *)
            decide true "$file is not documentation: every build runs."
            ;;
    esac
done <<< "$files"

decide false "Documentation only: the build jobs report without building."
