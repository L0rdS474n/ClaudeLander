# Changelog

All notable changes to ClaudeLander.  Format follows
[Keep a Changelog 1.1](https://keepachangelog.com/en/1.1.0/).
Versioning follows [SemVer 2.0](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed

- **Definition of Done now requires human verification** (ADR-0007,
  issue #19).  Tests passing, CI green, lint clean, and review
  approved are inputs to verification, never substitutes for it.  See
  `docs/adr/0007-dod-requires-human-verification.md`.

## v1.0.0 -- 2026-04-27 -- RETRACTED

> **Status: retracted on 2026-04-28.**
>
> The artefact tagged as `v1.0.0` produced a non-playable binary: the
> ship spawned pinned to the terrain, the camera sat below the
> landscape, the world rendered as a single triangle, projection
> collapsed everything to one pixel, mouse rotation was uncontrollable,
> the crash state never triggered, and the ship could not translate
> horizontally.  The release was tagged because **439 / 439 tests
> passed**; no human had ever launched the binary.
>
> Issues #9 -- #18 record the individual root causes spawned by the
> post-tag audit.  ADR-0007 (issue #19) introduces the human-verification
> gate that now blocks any future tag from repeating the failure.
>
> The GitHub release was deleted.  The git tag `v1.0.0` was deleted on
> origin.  Anyone who downloaded an artefact between the original tag
> time and the retraction should treat that artefact as broken and
> rebuild from the post-fix `main`.
