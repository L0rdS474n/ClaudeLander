# ADR 0001 -- MinGW toolchain link flags

**Status:** Accepted
**Date:** 2026-04-27
**Scope:** All Windows builds (cross-compile from Linux + native via MSYS2 /
MINGW64). Persists for the life of the project.

## Context

Pass 0 ships a CMake toolchain file
(`cmake/mingw-w64-x86_64.cmake`) for cross-compiling the game from a Linux
host to a Windows x86_64 target using `mingw-w64`. The original draft of
that file linked with:

```
-static-libgcc -static-libstdc++ -static -lpthread
```

The intent was to produce a self-contained `.exe` that does not require
the user to install a separate `libgcc_s_seh-1.dll` /
`libstdc++-6.dll` runtime. The Architecture Guardian flagged two
problems with this draft:

1. **`-static -lpthread` is wrong on MinGW-w64.** Modern `mingw-w64`
   toolchains do not ship a `libpthread.a`; they ship `libwinpthread`,
   which is already covered by `-static-libgcc`. The bare `-static`
   flag forces *all* libraries (including the OS-supplied OpenGL
   stub `libopengl32.a`) to be linked statically, which raylib's
   GLFW backend cannot satisfy.
2. **raylib + GLFW require explicit Windows DLL imports.** The
   GLFW backend that raylib uses on Windows calls into `opengl32.dll`,
   `gdi32.dll`, and `winmm.dll`. These must be on the link line, but
   they should be linked **dynamically** -- they ship with every
   Windows install since at least Windows XP and are never
   redistributed by application authors.

Without the explicit DLL imports, the cross-compile fails at link
time with undefined references to `glClear`, `wglMakeCurrent`,
`SwapBuffers`, `timeGetTime`, and similar symbols.

## Decision

**1. Toolchain file (`cmake/mingw-w64-x86_64.cmake`)** keeps only the
GCC runtime statically linked:

```cmake
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static-libgcc -static-libstdc++")
```

**2. `CMakeLists.txt`** adds the three Windows DLL imports as a normal
target link dependency, gated by `if(WIN32)`:

```cmake
if(WIN32)
    target_link_libraries(ClaudeLander PRIVATE opengl32 gdi32 winmm)
endif()
```

This is the conventional layout: toolchain configures the *runtime*,
target configures the *libraries the executable needs*.

## Consequences

* The produced `.exe` is self-contained for the **GCC runtime**:
  end users do not need `libgcc_s_seh-1.dll` or
  `libstdc++-6.dll` on their PATH. This is the property we wanted
  from `-static-libgcc -static-libstdc++` in the first place.
* The produced `.exe` depends, at run time, on `opengl32.dll`,
  `gdi32.dll`, and `winmm.dll`. **All three ship with every
  Windows installation since Windows XP.** No additional
  redistribution is needed.
* `if(WIN32)` covers both MinGW (cross + native via MSYS2) and
  MSVC, so a future MSVC build will not need a separate code path.
* The change is invisible on Linux: `if(WIN32)` evaluates to false
  and the block is skipped.

## Verification (deferred)

Per the user's hard constraint for Pass 0, the Windows build is
**not** executed locally. Verification happens in two later places:

* **CI -- GitHub-hosted Ubuntu runner with `mingw-w64`** (workflow
  `.github/workflows/ci-windows-cross.yml`): runs the cross-compile
  end-to-end and runs `file build-win/bin/ClaudeLander.exe`. The
  expected output line is:

  ```
  build-win/bin/ClaudeLander.exe: PE32+ executable (GUI) x86-64, for MS Windows
  ```

  The CI step asserts this string is present.
* **CI -- GitHub-hosted Windows runner with MSYS2 MINGW64** (workflow
  `.github/workflows/ci-windows-native.yml`): builds and runs ctest.
  No GUI run -- the smoke window would block CI.

Until those CI runs are green, this ADR records a *prepared* fix,
not a *verified* fix. Pass 0's exit criteria explicitly include
"prepared, not verified" for Windows.
