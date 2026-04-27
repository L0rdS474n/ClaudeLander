#!/usr/bin/env bash
# tests/scripts/check_pr_link_keyword.sh
#
# Pass 1.5 -- PR-link regex shell test suite
#
# Exercises the regex in scripts/check_pr_link.sh (Implementation deliverable)
# against each fixture file under tests/fixtures/pr_bodies/.
#
# Exit codes
#   0   all assertions passed
#   1   one or more assertions failed, or the check script does not exist yet
#
# Usage (from repo root or via ctest):
#   bash tests/scripts/check_pr_link_keyword.sh
#
# Environment variables accepted:
#   REPO_ROOT   override repo root (default: two levels up from this script)
#
# The script under test (scripts/check_pr_link.sh) must:
#   - Accept PR body on stdin
#   - Accept optional --title <string> flag to supply the PR title
#   - Exit 0  when the body OR title contains a valid issue-linking keyword
#   - Exit 1  when neither body nor title satisfy the pattern
#
# Coverage:
#   AC-1.5-087  PASS: body "Closes #1"
#   AC-1.5-088  PASS: body "Fixes #42"
#   AC-1.5-089  PASS: body "Resolves owner/repo#7"
#   AC-1.5-090  PASS: body "closes #5" (case-insensitive)
#   AC-1.5-091  PASS: empty body, title "Fixes #9" (keyword in title)
#   AC-1.5-087  FAIL: body "Some changes made to the codebase."
#   AC-1.5-088  FAIL: body "Refs #1" (non-closing keyword)
#   AC-1.5-089  FAIL: body "This closes the loop on the refactoring discussion."
#                          (keyword present but no issue number)

set -euo pipefail

# ---------------------------------------------------------------------------
# Resolve paths
# ---------------------------------------------------------------------------

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Default REPO_ROOT: two levels up from tests/scripts/
REPO_ROOT="${REPO_ROOT:-"$(cd "${SCRIPT_DIR}/../.." && pwd)"}"

CHECK_SCRIPT="${REPO_ROOT}/scripts/check_pr_link.sh"
FIXTURES_DIR="${REPO_ROOT}/tests/fixtures/pr_bodies"

# ---------------------------------------------------------------------------
# Guard: check_pr_link.sh must exist (Implementation deliverable)
# ---------------------------------------------------------------------------

if [[ ! -f "${CHECK_SCRIPT}" ]]; then
    echo "ERROR: ${CHECK_SCRIPT} does not exist." >&2
    echo "       This file is an Implementation deliverable for Pass 1.5." >&2
    echo "       Create it before running this test suite." >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# Colour helpers (graceful fallback when tty is absent)
# ---------------------------------------------------------------------------

if [[ -t 1 ]]; then
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[0;33m'
    RESET='\033[0m'
else
    RED='' GREEN='' YELLOW='' RESET=''
fi

PASS_COUNT=0
FAIL_COUNT=0

pass() { echo -e "${GREEN}PASS${RESET}  $*"; PASS_COUNT=$((PASS_COUNT + 1)); }
fail() { echo -e "${RED}FAIL${RESET}  $*"; FAIL_COUNT=$((FAIL_COUNT + 1)); }

# ---------------------------------------------------------------------------
# Helper: run the check script with a body file and optional title
# Returns the exit code of check_pr_link.sh.
# ---------------------------------------------------------------------------

run_check() {
    local body_file="$1"
    local title="${2:-}"

    if [[ -n "${title}" ]]; then
        bash "${CHECK_SCRIPT}" --title "${title}" < "${body_file}"
    else
        bash "${CHECK_SCRIPT}" < "${body_file}"
    fi
    return $?
}

# ---------------------------------------------------------------------------
# PASS fixtures: check script must exit 0
# ---------------------------------------------------------------------------

assert_pass() {
    local fixture_name="$1"
    local body_file="${FIXTURES_DIR}/${fixture_name}"
    local title="${2:-}"

    if [[ ! -f "${body_file}" ]]; then
        fail "[fixture missing] ${fixture_name}"
        return
    fi

    local exit_code=0
    run_check "${body_file}" "${title}" || exit_code=$?

    if [[ ${exit_code} -eq 0 ]]; then
        pass "${fixture_name}${title:+ (title: \"${title}\")} -> exit 0 (expected PASS)"
    else
        fail "${fixture_name}${title:+ (title: \"${title}\")} -> exit ${exit_code} (expected exit 0)"
    fi
}

# ---------------------------------------------------------------------------
# FAIL fixtures: check script must exit non-zero
# ---------------------------------------------------------------------------

assert_fail() {
    local fixture_name="$1"
    local body_file="${FIXTURES_DIR}/${fixture_name}"
    local title="${2:-}"

    if [[ ! -f "${body_file}" ]]; then
        fail "[fixture missing] ${fixture_name}"
        return
    fi

    local exit_code=0
    run_check "${body_file}" "${title}" || exit_code=$?

    if [[ ${exit_code} -ne 0 ]]; then
        pass "${fixture_name}${title:+ (title: \"${title}\")} -> exit ${exit_code} (expected FAIL)"
    else
        fail "${fixture_name}${title:+ (title: \"${title}\")} -> exit 0 (expected non-zero)"
    fi
}

# ---------------------------------------------------------------------------
# Test cases
# ---------------------------------------------------------------------------

echo "--- PR-link keyword regex tests ($(basename "${CHECK_SCRIPT}")) ---"
echo ""

# AC-1.5-087 subset: body contains "Closes #1"
assert_pass "pass_closes_simple.txt"

# AC-1.5-088 subset: body contains "Fixes #42"
assert_pass "pass_fixes.txt"

# AC-1.5-089 subset: body contains "Resolves owner/repo#7"
assert_pass "pass_resolves_cross_repo.txt"

# AC-1.5-090 subset: body contains lowercase "closes #5"
assert_pass "pass_lowercase.txt"

# AC-1.5-091 subset: empty body, keyword in title "Fixes #9"
TITLE_FIXTURE="${FIXTURES_DIR}/pass_title_only.title"
if [[ -f "${TITLE_FIXTURE}" ]]; then
    TITLE_CONTENT="$(cat "${TITLE_FIXTURE}")"
    # Strip trailing newline for --title argument
    TITLE_CONTENT="${TITLE_CONTENT%$'\n'}"
    assert_pass "pass_title_only.txt" "${TITLE_CONTENT}"
else
    fail "[fixture missing] pass_title_only.title"
fi

# FAIL: plain prose, no keyword
assert_fail "fail_no_keyword.txt"

# FAIL: "Refs #1" is not a closing keyword
assert_fail "fail_refs_only.txt"

# FAIL: keyword present but no issue number after it
assert_fail "fail_keyword_no_number.txt"

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------

echo ""
echo "Results: ${PASS_COUNT} passed, ${FAIL_COUNT} failed"

if [[ ${FAIL_COUNT} -gt 0 ]]; then
    exit 1
fi

exit 0
