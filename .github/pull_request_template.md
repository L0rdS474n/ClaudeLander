<!--
  ClaudeLander pull-request template
  See AGENTS.md and CONTRIBUTING.md for the full process.
  Reviewers: see Reviewer checklist at the bottom.
-->

## Summary

<!-- One or two sentences. What changed and why. -->

## Linked issue

<!--
  REQUIRED. Use one of GitHub's closing keywords on its own line so the
  enforce-issue-linked-pr workflow accepts the PR.
    Closes #N
    Fixes #N
    Resolves #N
  Cross-repo references are allowed:
    Resolves owner/repo#N
-->

Closes #

## Test evidence

<!--
  Paste the relevant ctest output (the tail is fine).  For UI-affecting
  changes, attach a screenshot or short video.  For changes that cannot
  be exercised in this pass, write DEFERRED and point at the pass that
  will exercise the change.
-->

```
ctest output goes here
```

## AC IDs covered

<!--
  List every AC-id this PR satisfies, one per line.  These must map to
  TEST_CASE prefixes in the test suite.
    - AC-1.5-001
    - AC-1.5-002
-->

- AC-

## ADR references

<!--
  If the change is shaped by an existing ADR, link it.  If the change
  introduces a new architectural decision, file the ADR in docs/adr/
  and link it here.
-->

- docs/adr/

## Reviewer checklist

<!-- Reviewer ticks each box before approving. -->

- [ ] PR body or title contains a closing keyword + issue number
      (`Closes #N`, `Fixes #N`, or `Resolves #N`).
- [ ] CI is green: Linux build+test, Windows cross-build,
      Windows native build+test, enforce-issue-linked-pr.
- [ ] Tests are present and named with the AC-id prefix; no test was
      weakened or skipped to make CI green.
- [ ] No outstanding-work markers (the four-letter task tag, the
      five-letter bug tag, "known limitation" wording) in code that
      this PR claims is complete.
- [ ] Real-world validation evidence is captured, or an explicit
      `DEFERRED` note points at the pass that will validate.
- [ ] Architecture boundary respected: no `#ifdef _WIN32` or
      `#ifdef __linux__` outside `src/platform/`; layer dependency
      graph in `docs/ARCHITECTURE.md` is unchanged or updated.
