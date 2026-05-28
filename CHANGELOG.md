# Changelog

All notable changes to Q1View are documented here.
Releases follow [semantic versioning](https://semver.org/) and are published to
the [GitHub Releases page](https://github.com/chammoru/Q1View/releases).

---

## [Unreleased]

---

## [2.0.0] — 2026-05-29

### Added
- Comparator: per-pane cursor coordinate readout (`C` to toggle). The hovered
  pane reports its own source-pixel coordinate; in "Allow Different
  Resolution" mode the other panes report the corresponding source-pixel
  position in their own image dimensions.
- Comparator: a per-pane close (X) button on each controls strip with hover
  highlight. Clicking releases just that pane's source while leaving the
  slot in place so a new file can be dropped onto it without changing the
  pane count.
- Comparator: image sequence support. Opening a still image now treats the
  rest of the folder's same-resolution images as a frame sequence — the
  timeline scales to the sequence length, Left/Right step frames, and Space
  plays through the sequence.
- Comparator: playback shortcuts (`Space`, `Left`, `Right`) now work from
  the timeline column and the bottom metric graph as well, not just the
  image canvases, so a click into either timeline view keeps keyboard
  control there.

### Changed
- Internal rename: `Comparer` → `Comparator` across the directory, project
  / solution files, classes (`CComparerDoc`, `CComparerView`, `ComparerPane`,
  …), resources, and `Comparator.exe` build output. The user-facing strings
  were already switched in 05af565; this completes the rename for internal
  identifiers. The MFC file-type ID `Comparer.Document` is preserved so file
  associations registered by older installs do not orphan.
- README now leads with the target audience and concrete workflows
  (codec / imaging / CV / QA / research) instead of a generic feature list.
- User guide documents the three secondary panes in Comparator — the left
  position timeline, the per-frame metric readout, and the bottom metric
  graph — including their click and playback behavior.

### Fixed
- MSVC C4533 / C4819 warnings in the Comparator and SMultithreadPlus
  sources (named `cursorPoint` initialization crossed by `goto`; missing
  UTF-8 BOM on files with non-ASCII characters under code page 949).

### Compatibility note
- Existing installs that ran the previous installer have a leftover
  `Comparer.exe` in the install directory. The new installer only writes
  `Comparator.exe`; the orphan file is harmless but stays until manual
  cleanup or reinstall. A future installer revision can add an explicit
  cleanup step.

---

## [1.0.16] — 2026-05-25

### Added
- Viewer shows source-native Y, U, V pixel values when inspecting raw YUV
  sources at high zoom. Press `V` to toggle between source YUV and display RGB.
  A corner badge identifies the active mode.

### Changed
- Pixel value component labels clarified to match the active display mode.

---

## [1.0.15] — 2026-05-25

### Added
- Raw YUV formats now support odd-width sources (e.g. 321×241). Previously only
  even widths were accepted.

### Changed
- Documentation and screenshots refreshed to match the current UI.

---

## [1.0.14] — 2026-05-24

### Changed
- Viewer video playback now follows a `QueryPerformanceCounter`-based
  presentation clock instead of a self-adjusting timer interval. Playback stays
  aligned with the source or user-selected FPS and recovers gracefully after
  device or UI stalls by dropping late frames.
- Playback timer is now cancelled synchronously on close to prevent use-after-
  free during shutdown.

---

## [1.0.13] — 2026-05-24

### Added
- Viewer context menu shows a check mark next to **Sync Input** when
  synchronization is active.

### Changed
- Sync Input pan operations are now coalesced before broadcast, reducing latency
  when panning quickly across multiple linked Viewer windows.

---

## [1.0.12] — 2026-05-24

### Added
- **Sync Input**: opt-in toggle in the Viewer right-click menu that synchronizes
  navigation, zoom, pan, rotation, playback, FPS, and display mode across all
  Viewer windows that have it enabled.

### Fixed
- Comparer now resets zoom and pan to a fitted, centered view when opening a new
  comparison set or when a replacement changes the working resolution. Zoom and
  pan are preserved when replacing a pane with a source of the same resolution.
- Comparer launched from Viewer now correctly opens the source file.
- Metric average labels no longer overflow their column at high DPI.

---

## [1.0.11] — 2026-05-24

### Fixed
- Video files on Unicode (including Korean) paths now open correctly in Viewer.
  OpenCV's `VideoCapture` is called through a temporary ASCII-safe copy path to
  work around its lack of Unicode support.

### Changed
- Legacy in-source TODO lists migrated to tracked GitHub Issues.
- Installer uninstall now cleans up residual files from the install directory.

---

## [1.0.10] — 2026-05-24

### Added
- Windows installer (`Q1ViewSetup-x64.exe`) built and signed by a GitHub Actions
  pipeline using Inno Setup. The installer registers separate Start Menu entries
  for Viewer and Comparer.
- AVIF / AV1 still image support documented; HEIF dependency archive updated to
  include the AOM decoder.

---

## [1.0.9] — 2026-05-24

### Added
- HEIF dependency archive rebuilt with the `aom` vcpkg feature enabled, adding
  AV1-backed AVIF decoding support.

### Changed
- The x265 HEVC encoder DLL is excluded from the dependency archive, reducing
  package size and removing an unused component.

---

[Unreleased]: https://github.com/chammoru/Q1View/compare/v1.0.16...HEAD
[1.0.16]: https://github.com/chammoru/Q1View/compare/v1.0.15...v1.0.16
[1.0.15]: https://github.com/chammoru/Q1View/compare/v1.0.14...v1.0.15
[1.0.14]: https://github.com/chammoru/Q1View/compare/v1.0.13...v1.0.14
[1.0.13]: https://github.com/chammoru/Q1View/compare/v1.0.12...v1.0.13
[1.0.12]: https://github.com/chammoru/Q1View/compare/v1.0.11...v1.0.12
[1.0.11]: https://github.com/chammoru/Q1View/compare/v1.0.10...v1.0.11
[1.0.10]: https://github.com/chammoru/Q1View/compare/v1.0.9...v1.0.10
[1.0.9]: https://github.com/chammoru/Q1View/releases/tag/v1.0.9
