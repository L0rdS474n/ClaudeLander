# Security policy

ClaudeLander is a clean-room reimplementation of a 1987 single-player
arithmetic game. It does not handle credentials, accept network input,
process untrusted file formats, or expose remote interfaces. The
realistic attack surface is small but not zero: the project depends on
raylib (a C library that links into the same address space as the game),
the C++ standard library, and the toolchain that builds it. The aim of
this document is to make it obvious how to report a vulnerability if you
find one, and what to expect after you do.

## Reporting a vulnerability

If you believe you have found a security issue in ClaudeLander -- a
buffer overflow, a malformed-input crash that the test suite does not
catch, an out-of-bounds read in a parsing path, a CMake/build-time
RCE vector via a malicious dependency, or anything similar --
please do not open a public issue.

Instead, send an email to the project maintainer:

  pomzm67@gmail.com

In the report, please include:

* A short description of the issue and where it lives in the codebase
  (file path, function, or commit hash if you have it).
* Steps to reproduce, ideally with a minimal example. Attach files if
  needed.
* The platform, compiler version, and raylib version you observed it
  on (Linux native, Windows MinGW cross-compile, or Windows MINGW64
  native).
* If known, your assessment of the impact (information disclosure,
  denial of service, code execution).

## Supported versions

The project is in active pre-release development; only the latest commit
on the `main` branch is supported. Once the first tagged release ships,
this section will be updated to list the supported tag range.

| Version          | Supported               |
|------------------|-------------------------|
| `main` (HEAD)    | yes                     |
| pre-release tags | not yet supported       |

## Response time

The maintainer acknowledges new reports within **5 business days** and
follows up with a triage outcome (accepted, needs more info, declined
with reason) within **15 business days** of acknowledgement. Fixes for
accepted reports are merged into `main` and called out in the next
release notes; if the fix is sensitive, the maintainer may coordinate
disclosure timing privately with the reporter.

## Out of scope

* Issues in dependencies (`raylib`, `Catch2`, `CMake`, MinGW-w64). Please
  report those upstream. If a dependency vulnerability materially
  affects ClaudeLander, we will track the upstream fix here.
* Reports about the game's clean-room boundary (e.g. "this code looks
  similar to the original"). Those are clean-room concerns and should
  be filed as a regular issue, not via the security channel.
* Theoretical issues without a reproducer. We will still read the
  report, but cannot prioritise it without something to verify.

Thank you for helping keep ClaudeLander honest.
