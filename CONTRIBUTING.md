# Contributing to ClaudeLander

Thank you for your interest in ClaudeLander -- a clean-room C++ / raylib
reimplementation of David Braben's *Lander* (Acorn Archimedes, 1987).

This document describes the contribution process for human contributors.
AI agents working on the codebase should additionally read
[`AGENTS.md`](AGENTS.md), which is the canonical agent contract.

## Getting started

1. Read [`README.md`](README.md) for the project overview and the build
   commands for your platform.
2. Read [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the layer
   dependency graph, coordinate convention (Y-DOWN end-to-end), and the
   single-flip-point rule.
3. Read [`AGENTS.md`](AGENTS.md) for the gate-driven workflow, coding
   standards, testing requirements, and Definition of Done. The rules
   apply to humans and to agents equally.
4. Browse the open issues and pick one labelled `good first issue` or
   `help wanted`. If you want to work on something not yet filed,
   open an issue first using the bug-report or feature-request template
   so the maintainers can confirm scope before you invest implementation
   time.
5. Fork the repository, create a feature branch (see "Branch strategy"
   below), and open a pull request when your change is ready for review.

## Branch strategy

There is no direct push to `main`. Every change lives on a feature
branch named with the literal pattern:

```
feature/pass-N-<topic>
```

`N` is the pass number (an integer that may include a half-step like
`1.5`), and `<topic>` is a short hyphenated description.

Examples:

* `feature/pass-1-core-math`
* `feature/pass-1.5-repo-collab`
* `feature/pass-2-world-terrain`

Pull requests targeting `main` must reference an issue using one of
GitHub's closing keywords on a dedicated line:

* `Closes #N`
* `Fixes #N`
* `Resolves #N`

Cross-repository references (`Resolves owner/repo#N`) are allowed.
The CI job `enforce-issue-linked-pr` blocks pull requests that do not
include such a reference in either the PR body or the title.

## Commit conventions

Commit messages follow [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>(<scope>): <short summary>

<optional longer body explaining the why>

<optional footer, e.g. Refs / Closes / breaking-change marker>
```

Common `<type>` values used in this project:

| Type       | Use for                                                     |
|------------|-------------------------------------------------------------|
| `feat`     | A new user-facing feature.                                  |
| `fix`      | A bug fix.                                                  |
| `docs`     | Documentation-only changes.                                 |
| `test`     | Test-only changes (adding or correcting tests).             |
| `refactor` | Behavioural-no-op restructuring.                            |
| `chore`    | Tooling, build config, repo housekeeping.                   |
| `ci`       | CI workflow changes.                                        |
| `perf`     | Performance change with measurable evidence.                |

Keep the subject line <= 72 characters. Prefer one logical change per
commit; squash noise locally before opening the PR.

## Running tests locally

ClaudeLander uses [Catch2 v3](https://github.com/catchorg/Catch2) (fetched
via CMake `FetchContent`, no system install required) plus a couple of
shell-based tests for the policy-script regex.

To run the full suite:

```bash
cmake -S . -B build -G Ninja
cmake --build build -j
ctest --test-dir build --output-on-failure
```

If your environment lacks the X11 development headers raylib requires,
add `-DCLAUDE_LANDER_TESTS_ONLY=ON` to skip raylib and the game target:

```bash
cmake -S . -B build-tests -G Ninja \
      -DCLAUDE_LANDER_BUILD_TESTS=ON \
      -DCLAUDE_LANDER_TESTS_ONLY=ON
cmake --build build-tests -j
ctest --test-dir build-tests --output-on-failure
```

The `--output-on-failure` flag prints failing test output inline -- always
include it when reporting test results in PR descriptions or bug reports.

If `ninja` is not on your PATH, drop the `-G Ninja` argument; CMake falls
back to the system default generator (Unix Makefiles on Linux).

## Filing issues

Issues use the templates in `.github/ISSUE_TEMPLATE/`:

* **Bug report** -- report unexpected behaviour. Include reproduction
  steps, expected vs. actual behaviour, and your environment
  (OS, compiler version, raylib backend).
* **Feature request** -- propose a new capability. Include motivation,
  proposed behaviour, and a draft acceptance-criteria list. Feature
  requests that are out of scope for the current pass are still
  welcome; they will be tagged for a later pass.

Blank issues are disabled (`.github/ISSUE_TEMPLATE/config.yml`); please
use one of the structured templates so reviewers have the context they
need.

For security vulnerabilities, see [`SECURITY.md`](SECURITY.md). Do not
report vulnerabilities through public issues.

## Pull-request review process

Every pull request is reviewed against the gate-driven pipeline described
in [`AGENTS.md`](AGENTS.md). The reviewer (human or agent) verifies:

* The PR body links an issue with `Closes #N`, `Fixes #N`, or
  `Resolves #N`. The CI job `enforce-issue-linked-pr` enforces this
  automatically.
* The change satisfies one objective; multi-objective PRs are split.
* New behaviour is covered by tests prefixed with the matching
  acceptance-criterion id.
* `ctest` output is captured in the PR description.
* No platform-conditional preprocessor logic outside `src/platform/`.
* No outstanding work markers (the four-letter task tag, the five-letter
  bug tag, the three-letter caution tag, or "known limitation" wording)
  remain in code that the PR claims is complete.
* For user-facing behaviour, real-world validation evidence (screenshot,
  short video, or an explicit `DEFERRED` note pointing at the pass that
  will validate) is included.
* **Human verification (ADR-0007) is recorded.**  No PR meets DoD
  unless a human has launched the produced artefact and confirmed in
  writing that it works as intended -- recording the verifier handle,
  the platform used, the commit verified, and what was checked.  Tests
  passing, CI green, and review approved are inputs to verification,
  never substitutes for it.  AI agents may not tick this box on their
  own; they must request a human verifier and wait for the written
  confirmation.  See
  [`docs/adr/0007-dod-requires-human-verification.md`](docs/adr/0007-dod-requires-human-verification.md)
  for the full rule and the v1.0.0 incident that drove it.
* The Reviewer checklist in `.github/pull_request_template.md` is
  explicitly ticked.

The pull-request template (`.github/pull_request_template.md`) lists the
required sections: Linked issue, Summary, Test evidence, AC IDs covered,
ADR references, Reviewer checklist. Pull requests opened from a fork
inherit the template automatically.

## Code of conduct

This project is governed by the
[Contributor Covenant 2.1](CODE_OF_CONDUCT.md). By contributing, you
agree to abide by its terms. Reports of unacceptable behaviour go to
the maintainer email listed in `CODE_OF_CONDUCT.md`.

## Licence

Contributions are accepted under the project's MIT licence (see
[`LICENSE`](LICENSE)). By opening a pull request you certify that your
contribution is your own work, or that you have permission to relicense
it under MIT.
