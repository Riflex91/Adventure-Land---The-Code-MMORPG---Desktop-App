# Build status

## Linux
- GCC 14.2 / C++23: PASS
- CMake 3.31: PASS
- SQLite 3.46: PASS
- X11 native window: PASS
- core SQLite/FTS test: PASS
- Xvfb render smoke test: PASS

Generated binary: `dist/linux/adventure-land-reference-os`

## Windows
A Win32/GDI native backend and build script are included. A Windows `.exe` was **not** cross-compiled in the current Linux environment because a MinGW/MSVC Windows toolchain is not installed here. Build with `build_windows.ps1` on Windows + Visual Studio + vcpkg.

## Current data scope
The bundled SQLite database is built from the curated V11.1.3 local seed: 45 items, 33 monsters, 53 skills, 7 classes and 8 maps. Full live-source parity is the next native porting step; this alpha does not claim the web app's 600+ live items are already embedded.
