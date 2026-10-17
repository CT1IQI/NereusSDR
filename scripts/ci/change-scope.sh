#!/usr/bin/env bash
# Decide whether a pull request needs the builds and the test suite.
#
# A pull request that changes only documentation skips the heavy work: the
# build jobs still start and report, so every required check passes, but they
# do nothing. Everything else, and every push and manual run, builds.
#
# Documentation means a file under docs/ or website/, or a *.md file at the
# top of the repository, that nothing outside those reads. A file anywhere
# else is never documentation, whatever its name: resources/help/*.md is
# compiled into the app through resources.qrc.
#
# "Reads" is checked for every documentation path: any tracked file outside
# docs/ and website/ (source, tests, CMake, the resource lists, tools/,
# hal-plugin/, scripts/, ...) that names the path or its folder on a line
# that is not a comment. A top-level file's name counts only on its own,
# not as the end of a longer path (docs/x/README.md does not name
# README.md). A folder counts when it is named on its own or with a file
# pattern after it ("icons/%1.svg", "icons/*.svg"), not as the start of
# another file's path: docs/architecture/a.md does not name the folder of
# docs/architecture/b.md. Files that build nothing are not searched: *.md files
# anywhere (they are text; one the app compiles in is named by its
# resource list, which is searched), .github/ (the release workflow and
# the pull request template; ci.yml's own checks run on every pull
# request), .gitattributes, .gitignore, CODEOWNERS, the top-level LICENSE
# texts and this script. tst_app_icon,
# for example, opens the icons in a docs/architecture design folder, so a
# change there has to run the tests. A comment that names a document (this
# is common: "see docs/architecture/...") reads nothing, so it does not
# count. Which lines are comments depends on the file's kind:
#   C, C++, Objective-C, Swift, JavaScript: "//" or "/*" before the path,
#     or a line starting with "*" (inside a block comment);
#   CMake, Python, shell, YAML, PowerShell, systemd units: "#" before the
#     path;
# (a configure template, x.in, is the kind of x);
#   anything else: no line is a comment, so every mention counts.
#
# Renames are listed as a deletion and an addition (--no-renames), so moving
# a source file into docs/ still names the source path.
#
# Run from the root of a checkout of the pull request's merge commit with at
# least two commits of history (actions/checkout fetch-depth: 2). Writes
# heavy=true or heavy=false to $GITHUB_OUTPUT, or to stdout without it, and
# says why. Anything unexpected (no parent, a failed diff, an empty list, a
# failed search) means heavy=true: a wrong "true" costs a build, a wrong
# "false" lets an untested change through.
#
#   scripts/ci/change-scope.sh <github.event_name>
#
# CI speed round, 2026-10-16; the review fixes (top-level *.md only,
# --no-renames, every path checked, the whole repository searched),
# 2026-10-17.
set -uo pipefail

out="${GITHUB_OUTPUT:-/dev/stdout}"

decide() {
    echo "heavy=$1" >> "$out"
    echo "$2"
    exit 0
}

# Prints the first line, outside the documentation itself, that names $1
# and is not a comment, as file:line:text. Status 0 if there is one, 1 if
# there is none, 2 if the search failed.
reader_of() {
    local hits status
    hits="$(git grep -n -F -e "$1" -- . ':(exclude)docs/' ':(exclude)website/' \
        ':(exclude,glob)**/*.md' ':(exclude)*.md' ':(exclude).github/' \
        ':(exclude).gitattributes' ':(exclude).gitignore' ':(exclude)CODEOWNERS' \
        ':(exclude)LICENSE*' ':(exclude)scripts/ci/change-scope.sh' 2>/dev/null)"
    status=$?
    if [ "$status" -eq 1 ]; then
        return 1
    fi
    if [ "$status" -ne 0 ]; then
        return 2
    fi
    awk -v needle="$1" '
        {
            # file:line:text; the file name has no colon in this repository.
            file = $0; sub(/:.*/, "", file)
            text = $0; sub(/^[^:]*:[0-9]+:/, "", text)
            # The first mention that is a whole name. A top-level file (no
            # "/" in it) preceded by a path or word character is the end of
            # a longer name. A folder (ending in "/") followed by a name
            # character is the start of another path, unless a file
            # pattern follows (%, *, $, {, <).
            rest = text; offset = 0; at = 0
            folder = (substr(needle, length(needle), 1) == "/")
            while ((i = index(rest, needle)) > 0) {
                prev = (offset + i > 1) ? substr(text, offset + i - 1, 1) : ""
                after = substr(text, offset + i + length(needle))
                whole = 1
                if (index(needle, "/") == 0 && prev ~ /[A-Za-z0-9_.\/-]/) {
                    whole = 0
                }
                if (folder && after ~ /^[A-Za-z0-9_.-]/ && after !~ /^[^\/ \t"'\''`)]*[%*${<]/) {
                    whole = 0
                }
                if (whole) {
                    at = offset + i
                    break
                }
                offset += i
                rest = substr(text, offset + 1)
            }
            if (at == 0) {
                next
            }
            before = substr(text, 1, at - 1)
            kind = "other"
            # A configure template (x.service.in, x.h.in) is its inner kind.
            sub(/\.in$/, "", file)
            if (file ~ /\.(c|cc|cpp|cxx|h|hh|hpp|hxx|m|mm|swift|js|ts|qml)$/) {
                kind = "slash"
            } else if (file ~ /(^|\/)CMakeLists\.txt$/ || file ~ /\.(cmake|py|sh|bash|ya?ml|ps1|toml|cfg|ini|service|socket|conf)$/) {
                kind = "hash"
            }
            if (kind == "slash" && (before ~ /\/\// || before ~ /\/\*/ || text ~ /^[ \t]*\*/)) {
                next
            }
            if (kind == "hash" && before ~ /#/) {
                next
            }
            print
            found = 1
            exit
        }
        END { exit found ? 0 : 1 }
    ' <<< "$hits"
}

if [ "${1:-}" != "pull_request" ]; then
    decide true "Not a pull request: every build runs."
fi

# The merge commit's first parent is the base branch, so this is exactly
# what the pull request changes.
if ! files="$(git diff --name-only --no-renames HEAD^1 HEAD 2>/dev/null)"; then
    decide true "Could not list the pull request's files: every build runs."
fi
if [ -z "$files" ]; then
    decide true "The pull request changes no files: every build runs."
fi

while IFS= read -r file; do
    case "$file" in
        docs/*|website/*)
            ;;
        */*)
            decide true "$file is not documentation: every build runs."
            ;;
        *.md)
            ;;
        *)
            decide true "$file is not documentation: every build runs."
            ;;
    esac
    # The file itself, and its folder for files read by folder (a test
    # that opens every icon in a folder names the folder, not each file).
    # A top-level file has no folder to check.
    needles=("$file")
    case "$file" in
        */*) needles+=("$(dirname "$file")/") ;;
    esac
    for needle in "${needles[@]}"; do
        reader="$(reader_of "$needle")"
        case $? in
            0) decide true "$file: $reader names $needle, so every build runs." ;;
            1) ;;
            *) decide true "$file: could not search for readers of $needle, so every build runs." ;;
        esac
    done
done <<< "$files"

decide false "Documentation only: the build jobs report without building."
