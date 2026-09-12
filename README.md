# Adventure Land Reference OS Desktop — C++23 Native Core

A speed-first native desktop edition of the Adventure Land reference project. It is deliberately **not** a WebView, Electron, Tauri, PHP or browser wrapper.

## Native stack

- C++23
- native Win32/GDI backend on Windows
- native X11 backend on Linux
- SQLite + FTS5 local search/index
- CMake
- Python only as a build-time database generator/downloader

## Current alpha

The native shell currently provides Dashboard, Items, Monsters, Skills, Classes, Maps, global full-text search, a local SQLite index, entity inspector, virtualized list rendering and native F11 fullscreen.

The build downloads the current curated Reference-OS seed from the live portfolio site and generates `resources/reference.db` during CI. This keeps generated database files out of Git while still producing a self-contained portable artifact. The complete Web V11 feature set is not yet ported to C++.

The seed URL can be overridden with the `AL_SEED_URL` environment variable.

## Automatic cloud builds

GitHub Actions builds both platforms automatically on every push to `main` and can also be started manually.

1. Open the repository's **Actions** tab.
2. Select **Build Desktop App**.
3. Select **Run workflow** for a manual build, or open the latest automatic run.
4. Download one of the artifacts:
   - `AdventureLandReferenceOS-Windows-x64`
   - `AdventureLandReferenceOS-Linux-x86_64`

The Windows artifact contains a portable `AdventureLandReferenceOS.exe`; the Linux artifact contains the native x86_64 binary. Both include their generated `resources/reference.db` and do not require PHP or a browser.

## Local Linux build

Requirements: CMake >= 3.21, a C++23 compiler, X11 development headers, SQLite3 development files, Python 3 and internet access for the build-time seed download.

```bash
./build_linux.sh
```

## Local Windows build

Recommended: Visual Studio 2022/2026 x64 and vcpkg. GitHub's Windows runner already contains vcpkg.

```powershell
.\build_windows.ps1
```

## Controls

- type anywhere: search
- Backspace: edit search
- Escape: clear search
- mouse wheel: list scroll
- F11: native fullscreen toggle

## Native roadmap

1. Official source sync/parser into normalized SQLite
2. Exact Drop Engine
3. Character + Combat Engine
4. World Atlas / geometry GPU layer
5. Acquisition/Crafting graph
6. Runtime/event simulator
7. Integrity + schema regression
8. Workspaces / snapshots / backup

The web application remains a separate frontend. The shared contract is verified Adventure Land data and algorithms, not UI code.
