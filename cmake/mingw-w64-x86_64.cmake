# MinGW-w64 cross-compile toolchain (Linux host -> Windows x86_64 target).
#
# Usage:
#   cmake -S . -B build-win -G Ninja \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake
#   cmake --build build-win -j
#
# Requires the `mingw-w64` package on the host (Debian/Ubuntu:
# `sudo apt install mingw-w64`).

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)

set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)
set(CMAKE_RC_COMPILER  ${TOOLCHAIN_PREFIX}-windres)

set(CMAKE_FIND_ROOT_PATH /usr/${TOOLCHAIN_PREFIX})

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Ship a self-contained .exe by statically linking the GCC runtime libraries
# (libgcc and libstdc++). We deliberately do NOT pass bare `-static` here: that
# would also static-link OS libraries (winpthread, the OpenGL stub, etc.) which
# is both unnecessary and tends to break the raylib + GLFW link line.
#
# OpenGL / GDI / WinMM are pulled in through `target_link_libraries(...)` in
# the top-level CMakeLists under `if(WIN32)`. See
# docs/adr/0001-mingw-toolchain-link-flags.md for the rationale.
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static-libgcc -static-libstdc++")
