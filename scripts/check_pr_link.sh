#!/usr/bin/env bash
# scripts/check_pr_link.sh
#
# Pass 1.5 -- enforce that every pull request links an issue using one of
# GitHub's closing keywords (Closes / Fixes / Resolves and conjugations)
# followed by a numeric issue reference (`#N` or `owner/repo#N`).
#
# Usage:
#
#   # body on stdin
#   bash scripts/check_pr_link.sh < pr_body.txt
#
#   # body on stdin, title via flag
#   bash scripts/check_pr_link.sh --title "Fixes #9" < empty_body.txt
#
#   # show help
#   bash scripts/check_pr_link.sh --help
#
# Exit codes:
#   0   PR body OR title contains a valid linking keyword + issue number
#   1   neither body nor title satisfies the pattern
#   2   bad invocation (unrecognised flag, missing argument)
#
# The regex (POSIX ERE, case-insensitive on input):
#
#   (close[sd]?|fix(e[sd])?|resolve[sd]?)[[:space:]]+
#       ([A-Za-z0-9_.-]+/[A-Za-z0-9._-]+)?#[0-9]+
#
# Conjugations matched:
#   close, closes, closed
#   fix,   fixes,  fixed
#   resolve, resolves, resolved
#
# Cross-repository references are accepted (`Resolves owner/repo#N`).
#
# This script is the single source of truth for the linking regex.  The
# GitHub Actions workflow `.github/workflows/enforce-issue-linked-pr.yml`
# delegates to it, and the shell test
# `tests/scripts/check_pr_link_keyword.sh` exercises it against fixtures
# in `tests/fixtures/pr_bodies/`.

set -euo pipefail

PROG="$(basename "$0")"

# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------

TITLE=""

print_help() {
    cat <<EOF
Usage:
  ${PROG} [--title <pr-title>] [--help]

Reads the pull-request body from standard input.  Returns exit 0 when the
body OR the optional --title contains an issue-linking keyword followed by
'#<number>' (or 'owner/repo#<number>').  Returns exit 1 otherwise.

Accepted closing keywords (case-insensitive):
  close[s|d], fix[es|ed], resolve[s|d]

Examples:
  echo "Closes #1"             | ${PROG}
  echo ""                      | ${PROG} --title "Fixes #9"
  echo "Resolves owner/repo#7" | ${PROG}

Exit codes:
  0  body or title links an issue with a closing keyword + #N
  1  no link found
  2  bad invocation
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --title)
            if [[ $# -lt 2 ]]; then
                echo "${PROG}: --title requires an argument" >&2
                exit 2
            fi
            TITLE="$2"
            shift 2
            ;;
        --title=*)
            TITLE="${1#--title=}"
            shift
            ;;
        --help|-h)
            print_help
            exit 0
            ;;
        --)
            shift
            break
            ;;
        *)
            echo "${PROG}: unrecognised argument: $1" >&2
            echo "Try '${PROG} --help'." >&2
            exit 2
            ;;
    esac
done

# ---------------------------------------------------------------------------
# Read the PR body from stdin
# ---------------------------------------------------------------------------

# `cat` with no args reads stdin; allow empty input.
BODY="$(cat || true)"

# ---------------------------------------------------------------------------
# Regex (POSIX ERE form, case-insensitive matching is delegated to grep -E -i)
# ---------------------------------------------------------------------------
#
#   keyword              optional cross-repo prefix      issue number
#   ──────────────────   ─────────────────────────────   ────────────
#   (close[sd]?          ([A-Za-z0-9_.-]+/                #[0-9]+
#    |fix(e[sd])?         [A-Za-z0-9._-]+)?
#    |resolve[sd]?)
#
# Whitespace between the keyword and the issue token may be any combination
# of spaces, tabs, or a newline (we use [[:space:]]+).
#
# We anchor the keyword to a non-word boundary on the left to avoid matches
# inside other words (e.g. "preclose#1" or "transfix #2" should not match).
# POSIX ERE has no \b; we approximate with (^|[^A-Za-z]) and check for a
# leading non-letter.

REGEX='(^|[^A-Za-z])(close[sd]?|fix(e[sd])?|resolve[sd]?)[[:space:]]+([A-Za-z0-9_.-]+/[A-Za-z0-9._-]+)?#[0-9]+'

# ---------------------------------------------------------------------------
# Match against body, then against title.  Either is sufficient.
# ---------------------------------------------------------------------------

match_text() {
    local text="$1"
    if [[ -z "${text}" ]]; then
        return 1
    fi
    # grep -E for ERE, -i for case-insensitive, -q to suppress output, -z to
    # treat input as a single record so multi-line bodies work.
    printf '%s' "${text}" | grep -E -i -q -z -- "${REGEX}"
}

if match_text "${BODY}"; then
    exit 0
fi

if [[ -n "${TITLE}" ]] && match_text "${TITLE}"; then
    exit 0
fi

# ---------------------------------------------------------------------------
# No match -- explain why on stderr to make CI logs actionable
# ---------------------------------------------------------------------------

cat >&2 <<EOF
${PROG}: no issue-linking keyword found.

The PR body or title must contain one of:

  Closes #<number>
  Fixes  #<number>
  Resolves #<number>

Conjugations (close/closes/closed, fix/fixes/fixed,
resolve/resolves/resolved) and cross-repository references
(Resolves owner/repo#<number>) are also accepted.

Edit the pull request and add a closing keyword + issue number on
its own line, then push or re-run the workflow.
EOF

exit 1
