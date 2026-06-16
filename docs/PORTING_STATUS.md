# Q1View Porting Status

This file captures the current cross-platform porting state so future Codex sessions can continue without rediscovering the context.

## Branches

- The cross-platform work has **landed on `master`**: Tier 1 (CMake core build,
  portability fixes, core smoke test and CI) and Tier 2 (experimental Qt viewer,
  Linux packaging, Qt CI) are both upstream. #54 tracked the merge gate that was
  crossed.
- The short-lived `cross-platform-core` branch has been **merged and deleted** —
  it no longer exists. All further work lands directly on `master`.
- Qt viewer feature-parity work is tracked in #63. Video playback, file-change
  auto-refresh, and multi-window Sync Input have landed. The Qt **Comparator**
  is out of scope (see "Out of scope" below), so the only remaining #63 item —
  verifying the *packaged* artifacts on each OS — is now automated in CI: every
  Qt build runs the deployed bundle / AppImage headlessly via `--selftest`.
- The Qt viewer stays additive: it builds behind the `Q1VIEW_BUILD_QT_VIEWER`
  CMake option (default OFF) and only *links* the shared core, so the MFC
  Windows product (`Viewer.sln` / `Comparator.sln`) is unaffected.

## Completed

### Windows CI/CD Release

On `master`, GitHub Actions builds the existing MFC Viewer/Comparator on `windows-2022` and creates releases for `v*` tags.

Release `v1.0.7` was created successfully:

- Release: https://github.com/chammoru/Q1View/releases/tag/v1.0.7
- Successful run: https://github.com/chammoru/Q1View/actions/runs/26212632555
- Assets:
  - `Viewer.exe`
  - `Comparer.exe`
  - `opencv_videoio_ffmpeg430_64.dll`
  - `Q1View-windows-x64.zip`

### Cross-platform Core

On `master`, a CMake build exists for the portable core subset.

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

On `master`, a Qt-based viewer app was added.

Target:

- `q1view_viewer_qt`

Current capabilities:

- Opens a Qt main window.
- Has `File > Open...`.
- Loads common image formats supported by `QImageReader`, such as PNG/JPG/BMP.
- Decodes HEIF/HEIC/AVIF through libheif when the build finds it (see
  `ViewerQt/HeifReader.cpp`). Optional: builds without libheif simply omit the
  feature. Windows reuses the prebuilt root the MSBuild product restores under
  `.deps`; Unix discovers it via pkg-config. The cross-platform `viewer-qt.yml`
  CI now installs libheif on every matrix platform (apt on Linux, Homebrew on
  macOS, the prebuilt decode-av1 root via `Ensure-HeifDependency.ps1` on
  Windows), so those builds enable HEIF and the Windows package ships the
  libheif runtime DLLs alongside the viewer.
- Accepts an image path as the first command-line argument.
- Has `File > Open Raw...`.
- Loads raw frames by asking for file, width, height, and color space.
- Recognizes YUV4MPEG2 (`.y4m`) containers, reads their header dimensions,
  color space, frame count, and FPS, and routes File/Open, drag/drop, and
  sibling-file navigation through the Y4M loader rather than the headerless raw
  path.
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
  - high-zoom per-pixel value overlay (RGB hex per cell, with a pixel grid)
  - selection-region capture ('S' mode rubber-band; Copy/Save As act on the crop)
  - viewport-painted image surface (`ImageView`): only the exposed region is
    scaled/drawn, so large images zoom to high factors without a giant pixmap
  - video-file playback (`VideoView`, QMediaPlayer/QVideoWidget) on an optional
    Qt6::Multimedia page, with transport bar (play/pause, seek, volume/mute);
    builds gracefully disabled when the module is absent. This covers the
    existing MFC/OpenCV video-file path (`VidCapFrmSrc`/`VidCapThread`); the MFC
    Viewer does not expose a separate webcam/capture-card preview feature.
  - file-change auto-refresh: a `QFileSystemWatcher` reloads the active frame in
    place when the source changes on disk, preserving zoom, pan, selection,
    rotation, and frame index (View ▸ Auto-Reload on Change; the cross-platform
    analogue of `FileChangeNotiThread` / `WM_RELOAD`)
  - multi-window Sync Input over a loopback UDP-multicast bus (`SyncChannel`,
    replacing the Windows `WM_COPYDATA` broadcast): mirrors frame/file
    navigation, zoom/pan, rotation, display options, colour space, resolution,
    FPS, and playback state to sibling instances (Navigate ▸ Sync Input).
    Playback sync covers raw-sequence play/stop and video play/pause; the video
    clock/position itself is not mirrored (each window plays on its own clock)

Out of scope (maintainer decision, 2026-06-17):

- **Comparator / ComparatorQt features are not being ported to Qt.** The
  cross-platform effort targets *Viewer* parity only; the Windows MFC
  Comparator remains the comparison tool. Phase 5 of
  [PORTING_ROADMAP.md](PORTING_ROADMAP.md) is therefore descoped, and #63's
  remaining Viewer work (packaged-artifact verification) is now automated in
  CI rather than done by hand. With Comparator excluded, the Qt Viewer is at
  feature parity with the MFC Viewer.

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
- No Qt Viewer prerelease is currently published. The earlier experimental
  `qt-viewer-v0.1.0` / `v0.2.0` prereleases and their tags were dropped when the
  branch was rebased, so those release links no longer exist. Cut a fresh one
  from `master` when ready by pushing a `qt-viewer-v*` tag (or running the
  "Release Qt Viewer" workflow).

Relevant files:

- `ViewerQt/main.cpp`
- `ViewerQt/MainWindow.h`
- `ViewerQt/MainWindow.cpp`
- `ViewerQt/ImageView.h`
- `ViewerQt/ImageView.cpp`
- `ViewerQt/RawOpenDialog.h`
- `ViewerQt/RawOpenDialog.cpp`
- `ViewerQt/VideoView.h` / `ViewerQt/VideoView.cpp` (optional Qt6::Multimedia video page)
- `ViewerQt/SyncChannel.h` / `ViewerQt/SyncChannel.cpp` (multi-window Sync Input bus)
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
# Headless raw-format smoke check (offscreen QPA); also runs in viewer-qt.yml.
ctest --test-dir build --build-config Release --output-on-failure
```

The Qt smoke check opens each committed raw fixture
([tests/fixtures/raw/](../Tests/fixtures/raw/)) through the viewer's
`--selftest` mode and asserts a clean exit. Regenerate the fixtures with
[tests/fixtures/gen_raw_fixtures.c](../Tests/fixtures/gen_raw_fixtures.c), or run
the checks without CMake via [tests/run_qt_smoke.sh](../Tests/run_qt_smoke.sh).

Local build note: GitHub Actions remains the cross-platform source of truth (Linux/macOS/Windows). This Windows machine now has VS2022 (MSBuild + CMake) and Qt 6.5.3, so the core and the Qt viewer can also be built and run locally.

## Next Suggested Steps

1. Qt viewer parity (#63) is complete for the in-scope feature set; the Qt
   Comparator is intentionally not ported (the MFC Comparator remains the
   comparison tool). Raw fixtures and automated Qt smoke checks have landed, and
   the packaged artifacts are now verified headlessly in CI (`--selftest` against
   the deployed bundle / AppImage), replacing the earlier manual clean-desktop
   check.
2. Improve packaging for `q1view_viewer_qt`:
   - Windows packaging uses `windeployqt`.
   - macOS packaging uses `macdeployqt`.
   - Linux packaging now builds an AppImage; next verify it manually on a clean Linux desktop.
3. Publish experimental Qt Viewer prereleases:
   - Push a tag like `qt-viewer-v0.1.0`.
   - Download files from https://github.com/chammoru/Q1View/releases after the release workflow finishes.
   - Current direct release page: https://github.com/chammoru/Q1View/releases/tag/qt-viewer-v0.1.0
4. Improve raw image workflows:
   - Done: representative raw fixtures live in
     [tests/fixtures/raw/](../Tests/fixtures/raw/) (yuv420, nv12, nv21,
     yuv420p10le, p010, grayscale, rgb888, bgr888, rgba8888, bgr565).
5. Automated Qt smoke check:
   - Done: `viewer-qt.yml` now runs `ctest` after the build, driving the viewer
     `--selftest` over the raw fixtures on Linux/macOS/Windows.
   - Remaining: still verify the Linux AppImage manually on a clean desktop.
6. Keep follow-up porting changes additive on `master` — each must only *link* the shared core, never fork the MFC product's pixel logic.

## Important Cautions

- The existing `Viewer` and `Comparator` apps are MFC/Win32 projects. Native macOS/Linux support requires a new UI layer rather than small compile fixes.
- The current Qt viewer is not yet at feature parity with the Windows Viewer/Comparator (tracked in #63); it should not be presented as feature-complete.
