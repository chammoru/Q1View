# Cross-Platform Roadmap (Linux / macOS)

Status: **In progress** — Phases 1 & 3 landed on `master` (#54); remaining Qt viewer/Comparator parity tracked in #63 · Owner: @chammoru · Last updated: 2026-06-07

This document is the single source of truth for bringing Q1View to Linux and
macOS. Individual phases are promoted into GitHub issues when work on them
actually begins; this file holds the overall plan and rationale.

## Decision

Develop cross-platform support **inside this repository (monorepo)**, behind a
short-lived working branch that is **merged back into `master`** — not kept as a
long-lived fork, and not split into a separate project. (This is what happened:
the `cross-platform-core` branch landed Tier 1·2 on `master` via #54 and was
then deleted.)

### Why not a long-lived branch

`master` keeps shipping the Windows product (recent commits are Microsoft Store
submissions). A divergent platform branch would drift from active development,
turning every shared-core fix into cherry-picks and the eventual merge into
merge hell.

### Why not a separate repository / submodule

The shared core is already substantial **and already written to be portable**,
so splitting repos adds version-sync and submodule friction with no real
benefit at this scale. A separate repo would only pay off with a different
release cadence / team / license, which is not the case here.

## Current state (measured)

| Layer | Directories | Size | Portability |
|---|---|---|---|
| **Core (shared)** | [QCommon](../QCommon), [QVisionCore](../QVisionCore), [SMultithreadPlus](../SMultithreadPlus) | ~6,665 LOC (~30%) | Already portable by design |
| **UI (Windows-only)** | [Viewer](../Viewer), [Comparator](../Comparator) | ~14,473 LOC (~70%) | MFC-bound (63 files), not reusable |

Evidence the core was already built with cross-platform in mind:

- `QCommon/inc/QPort.h` already branches on `__GNUC__` / `ANDROID` / `_WIN32`
  for sleep and atomics.
- `QVisionCore/Makefile` already builds `libQVisionCore.a` with gcc/g++.
- `QVisionCore/QCvUtil.cpp` guards `windows.h` behind `#ifdef _WIN32`.
- `SMultithreadPlus` already has `x86_windows`, `x86_linux_*`, and `linux_*`
  app directories.

So the non-portable part is essentially **only the MFC UI**. The core work is
"finish an abstraction that's already half-done," not "abstract from scratch."

## Phases

### Phase 1 — Build system: CMake for the core (low risk)

- [x] Add a top-level CMake build for `QCommon`, `QVisionCore`,
      `SMultithreadPlus` that builds on Windows, Linux, and macOS.
- [x] Do **not** touch the MFC apps yet — they keep their `.vcxproj` /
      `.sln`. CMake produces the core libraries they (and the future UI) link.
- [x] Verify the core compiles with gcc/clang on Linux (WSL is fine) and macOS.
      The existing `QVisionCore/Makefile` already proves the gcc path.

### Phase 2 — Core commonization (pays off regardless of platform)

- [ ] Ensure `Viewer` / `Comparator` consume the core **only through public
      headers** (`QCommon/inc`, `QVisionCore/*.h`) — no business logic in the
      MFC views.
- [ ] Audit `Viewer` / `Comparator` `.cpp` for image/codec logic that should
      move down into `QVisionCore`.
- [ ] Replace any remaining `#ifdef _WIN32` blocks that carry real logic with
      split files (`*_win.cpp` / `*_unix.cpp`) behind a common interface,
      rather than inline `#ifdef` sprawl. Example:

      FileUtil.h        // common interface
      FileUtil_win.cpp  // built on Windows only
      FileUtil_unix.cpp // built on Linux/macOS only

### Phase 3 — Cross-platform UI as a sibling app

- [x] Pick the UI toolkit. **Chose Qt (C++)** — the de-facto MFC replacement for
      pixel/imaging tools, links the existing C++ core with minimal binding cost,
      and covers Linux/macOS/Windows from one codebase. (Electron/Tauri are a
      poor fit for pixel-precise inspection.)
- [x] Add the new app as a sibling directory (`ViewerQt/`) in this repo, linking
      the shared core libraries from Phase 1.

### Phase 4 — Converge

- [x] Merged `cross-platform-core` into `master` and retired the branch (Tier 1·2 landed via #54).
- [ ] Keep the MFC apps as the mature Windows product; grow the Qt app
      alongside. Unifying all platforms on Qt is a later, larger bet.

## Open questions

- UI toolkit final call: Qt vs. native-per-OS (AppKit / GTK).
- macOS distribution target (notarization, App Store vs. direct).
- Whether the Qt app eventually replaces MFC on Windows too.
