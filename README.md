# Adventure Land Reference OS Desktop — Native C++23

Native Windows/Linux research application for Adventure Land. It is **not** a browser wrapper: no Electron, no Tauri, no WebView, no PHP and no Chromium.

## Architecture

- C++23
- Dear ImGui + GLFW + OpenGL 3.3
- SQLite + FTS5
- nlohmann/json
- embedded QuickJS-NG for the fixed Adventure Land LIVE SYNC source allowlist
- WinHTTP on Windows / libcurl on Linux
- local snapshots, workspaces and backups

The release build starts in native fullscreen by default. `F11` toggles fullscreen, `Esc` leaves fullscreen, and `Ctrl+K` opens the command palette. Navigation always shows **symbol + name**.

## Implemented Reference-OS feature set

### Data / entity system
- Dashboard
- Items, monsters, skills, classes, maps, NPCs
- Crafting, sets, token shops, dismantling
- Conditions, quests, events
- raw G.* inspector
- persistent entity context, recent entities, favorites and watchlist
- SQLite FTS5 global search
- Advanced Search DSL (`section:items type:weapon attack>100`, negation, comparisons, nested fields)
- Universal Compare
- Schema Explorer + local schema regression baseline

### Analysis tools
- Exact Drop Engine with recursive `open` pools
- Upgrade / Compound Lab
- recursive Craft Dependency Graph
- Monster Compare
- Equipment Build Designer
- Economy Lab
- Loot / Farm Calculator
- Character Build Compare
- Skill DPS / Heal projection
- Spawn Route Planner
- Item Acquisition Graph
- World / Spawn database
- Knowledge Graph
- Character / Combat Lab 2.0
- Data Integrity Scanner

### V10 / V11 / V11.1 native equivalents
- official-source LIVE SYNC, pinned to a GitHub revision when possible
- per-source SHA-256 manifest
- Source Version archive + restore
- Update Diff after LIVE SYNC
- Runtime / Event scenario layer (normal/hardcore, PvP, Halloween, Holiday Season, Lunar New Year, Valentines, Egg Hunt)
- Runtime layer feeds Exact Drops, Farming and Combat target data
- sanitized local Character Import
- imported observed stats for Combat Lab
- Geometry Atlas (`G.geometry` x/y collision lines) + derived local grid path probe
- Workspaces with JSON import/export
- Offline dataset snapshots + restore
- Native Self Test
- System Health
- one-file Backup / Restore (dataset + local state)
- Safe Mode: start with `--safe`

**Auction House is intentionally not included**; it remains an external project.

## Data model

Builds fetch the public Adventure Land definitions from:

`kaansoral/adventureland_mongodb`

The build evaluates the fixed `design/*.js` allowlist in a sandboxed Node VM, normalizes the resulting G.* objects, creates a local SQLite database and an FTS5 index. Runtime LIVE SYNC performs the same fixed allowlist flow natively through embedded QuickJS.

## GitHub Actions

Every push to `main` and every manual workflow run builds:

- `AdventureLandReferenceOS-Windows-x64`
- `AdventureLandReferenceOS-Linux-x86_64`

The workflow also generates the full official dataset and runs native core regression tests before artifacts are uploaded.

## Local build — Linux

Dependencies: CMake, Ninja, C++23 compiler, Node.js, Python 3, SQLite dev, nlohmann-json, GLFW, OpenGL and libcurl.

```bash
./build_linux.sh
```

## Local build — Windows

Use Visual Studio C++ + CMake + vcpkg, then:

```powershell
.\build_windows.ps1 -VcpkgRoot C:\vcpkg
```

The Windows build uses static SQLite/FTS5 and static GLFW through vcpkg.

## Command-line options

```text
--windowed       start in a normal window instead of fullscreen
--safe           ignore persisted UI state and open System Health
--reset-data     replace the local dataset with the packaged release dataset
--data-dir PATH  use a custom persistent data directory
```

Persistent application data is stored under `%LOCALAPPDATA%\AdventureLandReferenceOS` on Windows and `$XDG_DATA_HOME/AdventureLandReferenceOS` or `~/.local/share/AdventureLandReferenceOS` on Linux.
