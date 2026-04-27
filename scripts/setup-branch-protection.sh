#!/usr/bin/env bash
# scripts/setup-branch-protection.sh
#
# Pass 1.5 -- reproducibly apply ClaudeLander's branch-protection settings
# to the `main` branch on GitHub.
#
# This script is the single source of truth for the policy.  Re-running
# it after a settings drift restores the documented state.  See
# docs/adr/0003-repo-collaboration-policy.md (D4).
#
# Usage:
#
#   # default: read REPO_OWNER from `gh api user`, REPO_NAME=ClaudeLander
#   bash scripts/setup-branch-protection.sh
#
#   # override owner / repo via environment
#   REPO_OWNER=octocat REPO_NAME=Hello-World \
#       bash scripts/setup-branch-protection.sh
#
#   # show the JSON payload without calling the API
#   bash scripts/setup-branch-protection.sh --dry-run
#
#   # show this help text
#   bash scripts/setup-branch-protection.sh --help
#
# Exit codes:
#   0   protection applied (or dry-run printed payload, or --help)
#   1   API call failed, or required tooling missing
#   2   bad invocation (unrecognised flag)
#
# Environment:
#   REPO_OWNER   GitHub login or org that owns the repo
#                Default: `gh api user --jq .login` if `gh` is available
#   REPO_NAME    Repository name
#                Default: ClaudeLander
#   BRANCH       Branch to protect
#                Default: main
#
# Required protection fields (covered by AC-1.5-106):
#   required_status_checks
#   enforce_admins
#   required_pull_request_reviews
#   restrictions
#
# Required status checks (must match the four CI job names verbatim):
#   * Build + test (Ubuntu / GCC / Ninja)
#   * Cross-build (Ubuntu / MinGW-w64 / Ninja)
#   * Build + test (Windows / MSYS2 MINGW64 / Ninja)
#   * enforce-issue-linked-pr

set -e -u -o pipefail

PROG="$(basename "$0")"

# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------

DRY_RUN=0

print_help() {
    cat <<EOF
${PROG} -- apply ClaudeLander branch-protection settings to GitHub.

Usage:
  ${PROG} [--dry-run] [--help]

Environment variables:
  REPO_OWNER  GitHub login that owns the repository
              Default: \`gh api user --jq .login\`.
  REPO_NAME   Repository name (default: ClaudeLander).
  BRANCH      Branch to protect (default: main).

Flags:
  --dry-run   Print the JSON payload to stdout and exit 0 without
              calling the GitHub API.  Useful for review or for the
              ctest smoke test.
  --help      Show this help text and exit 0.

The required CI status checks are the four job names already produced
by the CI workflows in .github/workflows/.  They must match verbatim
(GitHub matches by check-run name, not by workflow file name).

This script depends on the GitHub CLI (\`gh\`); install it from
https://cli.github.com.  Authenticate once with \`gh auth login\`
before running.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --dry-run)
            DRY_RUN=1
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
# Resolve REPO_OWNER / REPO_NAME / BRANCH
# ---------------------------------------------------------------------------

REPO_NAME="${REPO_NAME:-ClaudeLander}"
BRANCH="${BRANCH:-main}"

if [[ -z "${REPO_OWNER:-}" ]]; then
    if command -v gh >/dev/null 2>&1 && [[ "${DRY_RUN}" -eq 0 ]]; then
        REPO_OWNER="$(gh api user --jq .login 2>/dev/null || true)"
    fi
    if [[ -z "${REPO_OWNER:-}" ]]; then
        # In dry-run mode (or when `gh` is not authenticated) we still
        # want to render the payload, so fall back to a placeholder
        # that the user can substitute later.
        REPO_OWNER="OWNER_TBD_PLACEHOLDER"
    fi
fi

# ---------------------------------------------------------------------------
# Build the JSON payload
# ---------------------------------------------------------------------------
#
# All four required-status-check names live in this single place; if the
# CI job names change, update both the workflow and this script.
#
# Other fields:
#   enforce_admins                 -> applies the rules to repo admins too
#   required_pull_request_reviews  -> a PR must exist before merging, but
#                                     `required_approving_review_count` is
#                                     0 because this is a solo-maintainer
#                                     project: a maintainer cannot approve
#                                     their own PR, and we still want CI
#                                     to gate every merge through a PR.
#                                     If you grow a team, raise this back to
#                                     1 (or higher) and re-enable
#                                     `require_code_owner_reviews`.
#   required_linear_history        -> reject merge commits on main
#   allow_force_pushes             -> false (no rewriting history on main)
#   allow_deletions                -> false (cannot delete protected branch)
#   restrictions                   -> null = no push restrictions beyond
#                                            the rules above
#
# `gh api` reads the payload from stdin when --input - is given.

PAYLOAD="$(cat <<'JSON'
{
  "required_status_checks": {
    "strict": true,
    "contexts": [
      "Build + test (Ubuntu / GCC / Ninja)",
      "Cross-build (Ubuntu / MinGW-w64 / Ninja)",
      "Build + test (Windows / MSYS2 MINGW64 / Ninja)",
      "enforce-issue-linked-pr"
    ]
  },
  "enforce_admins": true,
  "required_pull_request_reviews": {
    "dismiss_stale_reviews": true,
    "require_code_owner_reviews": false,
    "required_approving_review_count": 0,
    "require_last_push_approval": false
  },
  "restrictions": null,
  "required_linear_history": true,
  "allow_force_pushes": false,
  "allow_deletions": false,
  "block_creations": false,
  "required_conversation_resolution": false
}
JSON
)"

# ---------------------------------------------------------------------------
# Dry-run: print and exit
# ---------------------------------------------------------------------------

if [[ "${DRY_RUN}" -eq 1 ]]; then
    echo "# Branch protection payload (dry-run)"
    echo "# Target: ${REPO_OWNER}/${REPO_NAME} branch=${BRANCH}"
    printf '%s\n' "${PAYLOAD}"
    exit 0
fi

# ---------------------------------------------------------------------------
# Real run: require `gh`, then PUT the payload
# ---------------------------------------------------------------------------

if ! command -v gh >/dev/null 2>&1; then
    echo "${PROG}: GitHub CLI (\`gh\`) is required but not installed." >&2
    echo "Install it from https://cli.github.com and authenticate with" >&2
    echo "\`gh auth login\`, then re-run this script." >&2
    exit 1
fi

# Confirm the user is authenticated.
if ! gh auth status >/dev/null 2>&1; then
    echo "${PROG}: \`gh\` is not authenticated.  Run \`gh auth login\` first." >&2
    exit 1
fi

# Confirm the placeholder has been replaced.
if [[ "${REPO_OWNER}" == "OWNER_TBD_PLACEHOLDER" ]]; then
    echo "${PROG}: REPO_OWNER is still the placeholder." >&2
    echo "Set REPO_OWNER explicitly, e.g.:" >&2
    echo "  REPO_OWNER=your-handle ${PROG}" >&2
    exit 1
fi

API_PATH="repos/${REPO_OWNER}/${REPO_NAME}/branches/${BRANCH}/protection"

echo "Applying branch protection: ${REPO_OWNER}/${REPO_NAME} branch=${BRANCH}"
printf '%s\n' "${PAYLOAD}" | gh api \
    --method PUT \
    -H "Accept: application/vnd.github+json" \
    -H "X-GitHub-Api-Version: 2022-11-28" \
    --input - \
    "${API_PATH}"

echo "Done.  To verify:"
echo "  gh api ${API_PATH} | jq ."
