# Q1View Porting Status

This file captures the current cross-platform porting state so future Codex sessions can continue without rediscovering the context.

## Branches

- `master` must stay on the Windows release workflow baseline unless explicitly asked otherwise.
- `master` currently points to `f6b9b8c9a91c3cee6b2f77214ad831893a1a5626` (`Use VS2022 toolset in CI`).
- Native macOS/Linux porting work is on `cross-platform-core`.
- `cross-platform-core` has continued past the initial note with Qt packaging and raw image loading work.

Do not push experimental porting commits directly to `master`.

## Completed

### Windows CI/CD Release

On `master`, GitHub Actions builds the existing MFC Viewer/Comparer on `windows-2022` and creates releases for `v*` tags.

Release `v1.0.7` was created successfully:

- Release: https://github.com/chammoru/Q1View/releases/tag/v1.0.7
- Successful run: https://github.com/chammoru/Q1View/actions/runs/26212632555
- Assets:
  - `Viewer.exe`
  - `Comparer.exe`
  - `opencv_videoio_ffmpeg430_64.dll`
  - `Q1View-windows-x64.zip`

### Cross-platform Core

On `cross-platform-core`, a CMake build now exists for the portable core subset.

Targets:

- `q1view_image_core`
- `smultithreadplus`
- `q1view_core_smoke`

The core smoke test builds and passes on:

- `ubuntu-latest`
- `macos-latest`
- `windows-2022`

Successful run:

- https://github.com/chammoru/Q1View/actions/runs/26216905333
- Latest verified run: https://github.com/chammoru/Q1View/actions/runs/26230651185

Relevant files:

- `CMakeLists.txt`
- `.github/workflows/cmake-core.yml`
- `tests/core_smoke.cpp`
- `QCommon/inc/QPort.h`
- `QVisionCore/qimage_cs.c`
- `SMultithreadPlus/inc/SMutex.h`
- `SMultithreadPlus/src/SThread.cpp`

### Experimental Qt Viewer

On `cross-platform-core`, a minimal Qt-based viewer app was added.

Target:

- `q1view_viewer_qt`

Current capabilities:

- Opens a Qt main window.
- Has `File > Open...`.
- Loads common image formats supported by `QImageReader`, such as PNG/JPG/BMP.
- Accepts an image path as the first command-line argument.
- Has `File > Open Raw...`.
- Loads raw frames by asking for file, width, height, and color space.
- Supports command-line raw metadata:
  - `q1view_viewer_qt --raw --width 1920 --height 1080 --format yuv420 frame.yuv`
- Converts raw formats through `q1view_image_core`/`qimage_cs`.
- Remembers the last raw width, height, color space, and file path with `QSettings`.
- Supports drag/drop:
  - Known image files open directly.
  - Unknown local files open the raw dialog with the file path prefilled.
- Implements the first Viewer parity pass:
  - mouse wheel / trackpad zoom
  - left-drag panning
  - actual size and fit-to-window actions
  - full-screen toggle
  - rotate clockwise
  - Y-only display toggle
  - cursor coordinate and RGB status display
  - copy, paste, close, and save-as actions
  - previous/next file navigation
  - raw multi-frame navigation and timer playback

It does not yet implement:

- Video playback.
- Selection-region capture.
- High-zoom per-pixel text overlay.
- Comparer features.

Successful Qt viewer CI runs:

- https://github.com/chammoru/Q1View/actions/runs/26217369518
- https://github.com/chammoru/Q1View/actions/runs/26224574718
- https://github.com/chammoru/Q1View/actions/runs/26224816758
- Latest verified run: https://github.com/chammoru/Q1View/actions/runs/26233937957
- Latest mac interaction hardening run: https://github.com/chammoru/Q1View/actions/runs/26339947245

Artifacts were produced for:

- `q1view-viewer-qt-windows-2022`
- `q1view-viewer-qt-ubuntu-latest`
- `q1view-viewer-qt-macos-latest`

Latest artifact notes:

- Windows and macOS artifacts include Qt deployment output from `windeployqt`/`macdeployqt`.
- Linux now uploads an AppImage/AppDir package built with `linuxdeploy` and `linuxdeploy-plugin-qt`.
- The experimental Qt Viewer can be published as a GitHub prerelease with tags named `qt-viewer-v*`.
- First Qt Viewer prerelease: https://github.com/chammoru/Q1View/releases/tag/qt-viewer-v0.1.0
- Latest Qt Viewer prerelease: https://github.com/chammoru/Q1View/releases/tag/qt-viewer-v0.2.0
- Next Qt Viewer prerelease should be `qt-viewer-v0.2.1` for mac interaction fixes.

Relevant files:

- `ViewerQt/main.cpp`
- `ViewerQt/MainWindow.h`
- `ViewerQt/MainWindow.cpp`
- `ViewerQt/RawOpenDialog.h`
- `ViewerQt/RawOpenDialog.cpp`
- `.github/workflows/viewer-qt.yml`
- `.github/workflows/release-qt-viewer.yml`
- `packaging/linux/q1view-viewer-qt.desktop`
- `packaging/linux/q1view-viewer-qt.svg`
- `CMakeLists.txt`

## Current Build Commands

Core smoke test:

```sh
cmake -S . -B build -DQ1VIEW_BUILD_SMOKE_TEST=ON
cmake --build build --config Release --parallel
ctest --test-dir build --build-config Release --output-on-failure
```

Qt viewer:

```sh
cmake -S . -B build -DQ1VIEW_BUILD_SMOKE_TEST=OFF -DQ1VIEW_BUILD_QT_VIEWER=ON
cmake --build build --config Release --target q1view_viewer_qt --parallel
```

Local machine note: this Windows machine did not have `cmake`, `cl`, `gcc`, or `clang` available during the session, so validation was done through GitHub Actions.

## Next Suggested Steps

1. Keep working on `cross-platform-core`.
2. Improve packaging for `q1view_viewer_qt`:
   - Windows packaging uses `windeployqt`.
   - macOS packaging uses `macdeployqt`.
   - Linux packaging now builds an AppImage; next verify it manually on a clean Linux desktop.
3. Publish experimental Qt Viewer prereleases:
   - Push a tag like `qt-viewer-v0.1.0`.
   - Download files from https://github.com/chammoru/Q1View/releases after the release workflow finishes.
   - Current direct release page: https://github.com/chammoru/Q1View/releases/tag/qt-viewer-v0.1.0
4. Improve raw image workflows:
   - Add validation/test fixtures for representative raw formats.
5. Add a minimal automated Qt smoke check if practical:
   - For now CI only compiles and uploads artifacts.
6. Only after the branch is stable, open a PR from `cross-platform-core` to `master`.

## Important Cautions

- The existing `Viewer` and `Comparer` apps are MFC/Win32 projects. Native macOS/Linux support requires a new UI layer rather than small compile fixes.
- The current Qt viewer is intentionally minimal and should not be presented as feature-complete.
- Do not move `master` ahead with porting commits unless explicitly requested.
