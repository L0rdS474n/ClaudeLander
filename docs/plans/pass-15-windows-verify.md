# Pass 15 Windows MinGW Cross-Compile Verification

**Date:** 2026-04-27
**Status:** PREPARED — actual CI run deferred to Pass 16 push.

## 1. Scope

Verify Windows MinGW cross-compile and Windows-native build are wired
correctly in CI. Per user's hard constraint, no local installs are
permitted; verification of the actual cross-compile happens on GitHub
Actions runners after Pass 16 push.

## 2. Artifacts in place (verified)

| File | Purpose |
|------|---------|
| `cmake/mingw-w64-x86_64.cmake` | MinGW-w64 toolchain file (Linux→Windows cross). |
| `.github/workflows/ci-linux.yml` | Linux native build + tests. |
| `.github/workflows/ci-windows-cross.yml` | Linux→Windows cross-compile via MinGW (produces .exe). |
| `.github/workflows/ci-windows-native.yml` | Windows-native verify on `windows-latest` runner. |

## 3. Local verification (limited, per no-install constraint)

- [x] Toolchain file syntactically valid (CMake `include()` succeeds).
- [x] All three workflow YAML files parse (no `yamllint` errors locally; verified via `python -c` import.)
- [x] No `mingw-w64-g++` invocation attempted locally (would need `apt install`).

## 4. CI verification (deferred to Pass 16)

After `git push` of feature branches and `main`, the following must
turn green on GitHub Actions:

- [ ] `ci-linux.yml`: native gcc-13 + clang-17 build, `ctest` 378/378.
- [ ] `ci-windows-cross.yml`: MinGW cross-compile produces `ClaudeLander.exe`. Tests not run (cross-compiled).
- [ ] `ci-windows-native.yml`: Windows-native build on `windows-latest`, `ctest` 378/378.

## 5. Acceptance criteria

- AC-W01 — All four artifact files exist (verified 2026-04-27).
- AC-W02 — Toolchain file specifies `CMAKE_SYSTEM_NAME=Windows` and `CMAKE_SYSTEM_PROCESSOR=AMD64`.
- AC-W03 — All three CI workflows reference `CLAUDE_LANDER_BUILD_GAME=ON` for the executable build.
- AC-W04 — `ci-linux.yml` installs `libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev` (raylib X11 dependencies).
- AC-W05 — Pass 16 push triggers all three workflows.

## 6. Definition of Done (Pass 15)

- [x] Local artifact verification complete.
- [ ] Actual CI runs green — deferred to Pass 16.

## 7. Out of scope

- Local MinGW build (user constraint).
- Manual installation of any system packages.
- Verifying that the .exe runs on Windows — that requires user's
  Windows machine and is part of the manual playtest checklist
  documented in Pass 14.
