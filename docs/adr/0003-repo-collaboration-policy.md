# ADR 0003 -- Repository collaboration policy

**Status:** Accepted
**Date:** 2026-04-27
**Scope:** Pass 1.5 (Repo collaboration setup). Persists for the life of the
project.

## Context

Pass 0 produced a green build pipeline (CMake + raylib + Catch2 + three CI
workflows). Pass 1 began landing core math. Before more code lands, the
repository needs an explicit, machine- and human-readable collaboration
contract:

* Multiple AI coding agents (Claude Code, Cursor, GitHub Copilot, Aider,
  OpenAI Codex, GPT, Gemini) may be pointed at the repository. Each tool
  should pick up the same rules without per-tool configuration drift.
* The repository follows a clean-room policy with respect to David Braben's
  original *Lander* and Mark Moxon's reverse-engineered commentary at
  `lander.bbcelite.com`. That policy must be visible at the top level so a
  reader hitting the repo from any direction sees it.
* GitHub-side controls (branch protection, required status checks, issue
  linking, CODEOWNERS) need to be reproducible from a script so the project
  maintainer can re-apply them after migration or a settings reset.
* Contributor-facing documents (`CONTRIBUTING.md`, `CODE_OF_CONDUCT.md`,
  `SECURITY.md`) need to exist before the first non-bootstrap PR review,
  so reviewers and reporters have a documented surface to point at.

The existing repository state going into Pass 1.5:

* `docs/adr/0000-pass0-gate-skips.md` -- one-shot Pass 0 skip authorisation.
* `docs/adr/0001-mingw-toolchain-link-flags.md` -- toolchain decision.
* ADR-0002 lives on a parallel feature branch (`feature/pass-1-core-math`)
  and will be present in the trunk after that branch merges; this ADR does
  not depend on it.

## Decision

Pass 1.5 records seven decisions. Each one is independently testable and is
covered by acceptance criteria in `tests/test_repo_policy.cpp`.

### D1 -- AGENTS.md is the canonical agent contract

`AGENTS.md` at the repo root is the single source of truth for any AI agent
working on the codebase. It is written in English (so any tool can parse it),
covers project overview, clean-room policy, branch strategy, pipeline gates,
build instructions, coding standards, testing requirements, real-world
validation, anti-fabrication rules, Definition of Done, reference docs, and
acknowledgements. Tool-specific configuration files (e.g. `.cursorrules`,
`CLAUDE.md`) may exist but must defer to `AGENTS.md` on substantive rules.

### D2 -- Branch strategy: feature/pass-N-<topic>

No direct push to `main`. Every change lives on a `feature/pass-N-<topic>`
branch and reaches `main` through a pull request that links its issue with
`Closes #N` / `Fixes #N` / `Resolves #N`.

### D3 -- Issue linking is enforced by CI

Every PR must reference an issue with one of the GitHub closing keywords
(`Closes`, `Fixes`, `Resolves`, plus their conjugations) followed by
`#<number>` or `<owner>/<repo>#<number>`. The check runs as a workflow
(`.github/workflows/enforce-issue-linked-pr.yml`) backed by a portable shell
script (`scripts/check_pr_link.sh`). The shell script is testable in isolation
against fixtures in `tests/fixtures/pr_bodies/`, which keeps the CI logic
unit-testable without spinning up an Actions runner. The accepted regex
(case-insensitive, ERE form):

```
(close[sd]?|fix(e[sd])?|resolve[sd]?)[[:space:]]+([A-Za-z0-9_.-]+/[A-Za-z0-9._-]+)?#[0-9]+
```

### D4 -- Branch protection is reproducible

`scripts/setup-branch-protection.sh` is the single point of truth for
GitHub branch-protection settings on `main`. It is idempotent, supports
`--dry-run` (prints the JSON payload without calling the API), and `--help`.
The required status checks are the four CI job names that already exist:

* `Build + test (Ubuntu / GCC / Ninja)`
* `Cross-build (Ubuntu / MinGW-w64 / Ninja)`
* `Build + test (Windows / MSYS2 MINGW64 / Ninja)`
* `enforce-issue-linked-pr`

Other protections enabled: required PR reviews (1 approving review),
strict status checks, linear history, no force pushes, no deletions,
delete-branch-on-merge.

### D5 -- CODEOWNERS uses a placeholder until the GitHub repo exists

The repository has not been created on GitHub at the time Pass 1.5 lands
(per user constraint: no `gh repo create`). `CODEOWNERS` therefore lists
`@OWNER_TBD_PLACEHOLDER` as the default owner. Pass 16 (the GitHub repo
bring-up pass) replaces the placeholder with the maintainer's actual handle.

### D6 -- Contributor Covenant 2.1 is the code of conduct

`CODE_OF_CONDUCT.md` adopts Contributor Covenant version 2.1 **by reference**
(URL link to the canonical text) rather than transcribing the document body.
Rationale: the Covenant is itself a versioned upstream artefact maintained
under CC BY 4.0; pinning by URL keeps the project's CoC bytes-stable, lets
the upstream maintainers correct typos, and makes future re-pinning
(e.g. to 2.2) a single-line change. Contact email is the project
maintainer (`pomzm67@gmail.com`).

### D7 -- Issue templates restrict the front door

`.github/ISSUE_TEMPLATE/{bug_report,feature_request}.md` shape inbound
issues, and `.github/ISSUE_TEMPLATE/config.yml` disables blank issues so
every reported item starts from a structured template.

## Consequences

* New contributors (human or agent) have one canonical document to read
  (`AGENTS.md`) and a hard-coded branch + PR + issue-link workflow.
* The PR-link enforcement is testable locally via
  `bash tests/scripts/check_pr_link_keyword.sh`, which means the regex can
  be debugged without a GitHub round trip.
* Branch protection drifts in the GitHub UI are recoverable: re-running
  `scripts/setup-branch-protection.sh` restores the documented state.
* The `@OWNER_TBD_PLACEHOLDER` token in `CODEOWNERS` will produce a
  GitHub UI warning until Pass 16 replaces it; that is a documented
  expected state, not a defect.
* Pass 16 inherits a non-trivial real-world validation backlog: actually
  applying branch protection against a real repo, opening a probe PR
  to confirm the linking workflow blocks unlinked PRs, and verifying
  the four CI status check names match production exactly.

## Verification

Pass 1.5 verifies the **policy artefacts** (file existence, required
content, regex correctness, script structural shape):

* `tests/test_repo_policy.cpp` -- 57 Catch2 cases covering AC-1.5-001..202.
* `tests/scripts/check_pr_link_keyword.sh` -- 8 shell fixtures exercising
  the PR-link regex against pass / fail bodies and titles.
* `ctest --test-dir build --output-on-failure` runs all of the above
  alongside `setup-branch-protection.sh --dry-run` and `--help` smoke
  tests.

Real-world validation -- whether branch protection on the actual GitHub
repository is set correctly, whether the `enforce-issue-linked-pr` job
actually blocks PRs without `Closes #N`, whether the four CI status check
names line up with what GitHub Actions reports -- is **deferred to
Pass 16** (GitHub repo bring-up). The decision to defer is recorded
here so the deferral is explicit and the verification gap is visible.

## References

* ADR-0001 (`docs/adr/0001-mingw-toolchain-link-flags.md`) -- the only
  prior ADR present on this branch. ADR-0002 lives on
  `feature/pass-1-core-math` and will appear after that branch merges.
* GitHub closing-keyword reference:
  `https://docs.github.com/en/issues/tracking-your-work-with-issues/linking-a-pull-request-to-an-issue`
* Contributor Covenant 2.1:
  `https://www.contributor-covenant.org/version/2/1/code_of_conduct.html`
