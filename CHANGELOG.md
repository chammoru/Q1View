# Changelog

All notable changes to Q1View are documented here. Releases follow [semantic versioning](https://semver.org/) and are published to the [GitHub Releases page](https://github.com/chammoru/Q1View/releases).

---

## [Unreleased]

### Added
- Comparator: right-click now opens a popup menu with a checkable `Sel Mode` toggle (the same region-selection capture as the `S` shortcut) and a `Clear Selection` item for the synchronized rectangle (enabled only when a selection exists), mirroring the Viewer's right-click menu. Previously right-click only cleared the selection and selection mode was reachable solely via the keyboard.

---

## [2.6.0] — 2026-06-17

### Added
- Viewer: the thumbnail drawer is now resizable — drag the divider between the drawer and the image to widen it (handy for reading long file names) or narrow it. The drawer always lives inside the fixed window, so dragging the divider, or opening and closing the drawer, resizes the image area to fit and never changes the outer window size; the chosen width is remembered across sessions. The MFC and Qt viewers now share one drawer width default (and min/max) and behave identically — previously the MFC drawer could not be resized at all and grew the window when opened.

### Changed
- Viewer: the default window now opens at the standard 640×480 (VGA) instead of the previous non-standard 500×392, giving a roomier workspace on modern displays while keeping the size fixed — loading an image or selecting a thumbnail does not resize the window, and a manually resized window is still remembered across sessions. The MFC and Qt viewers share one default, which also seeds the assumed dimensions for raw images of unknown size. (issue #77)

### Fixed
- Viewer: selecting a thumbnail from the browser no longer resizes the window to fit each image. Like folder navigation, it now keeps the current window size and fits the chosen image into the existing viewport, so stepping through a folder of mixed-resolution images stays smooth instead of making the window jump, maximize, and flicker (which also left thumbnails briefly blank during the resize churn). (issue #76)

---

## [2.5.0] — 2026-06-14

### Added
- Comparator: a synchronized selection rectangle for side-by-side region inspection. Press `S` to enter selection mode and drag a rectangle on either image; Comparator draws the same region on both panes so the corresponding area can be compared directly, and the rectangle stays locked to that image region while zooming, panning, or resizing. A width × height readout is shown next to the rectangle, and the selection clears with `Esc` or a right-click. The selected region's score for the active metric (PSNR/SSIM, computed on the color-space-native planes, or LPIPS on the cropped region) is shown next to the whole-image score, so localized artifacts can be told apart from the global figure.
- Comparator: a selectable LPIPS perceptual backbone. The Metric menu now offers `LPIPS-AlexNet` (the existing fast, lightweight default) and `LPIPS-VGG` (a heavier, more detailed mode), each downloaded on demand the first time it is selected and verified by a pinned SHA-256. Results always carry the backbone in the label (for example `LPIPS-VGG(0.0020)`) since values from different backbones are not interchangeable, each backbone keeps its own per-frame cache, and the chosen metric is remembered across sessions.

### Packaging
- The app, taskbar, and Start menu icons now render larger within their frame. The SVG masters' built-in safe margin was reduced (the rounded square grows from ~84% to ~91% of the icon), so the artwork reads bigger at every size while keeping the same glyph proportions and rounded corners. Regenerated from the `Q1View.svg` / `Comparator.svg` masters.

---

## [2.4.4] — 2026-06-14

### Packaging
- Microsoft Store submissions now pin the listing description and short description to the curated text in `docs/STORE_LISTING.md` whenever release notes are updated, preventing stale Partner Center metadata with hard-wrapped mid-sentence line breaks from being resubmitted.

---

## [2.4.3] — 2026-06-14

### Packaging
- The taskbar and Start menu now show the rounded Q1View icon directly instead of a square plate behind it. The Microsoft Store package is now built with a resource index (`resources.pri`) so Windows can select the unplated icon assets that shipped in 2.4.1 but were previously never used, and the icon renders at full size again.

---

## [2.4.2] — 2026-06-14

### Packaging
- Taskbar-sized icon frames now include extra transparent safe padding, so the rounded Q1View corners stay visible at small Windows taskbar sizes instead of reading as a nearly square blue block.

---

## [2.4.1] — 2026-06-14

### Fixed
- Viewer: Ctrl+V now pastes the actual clipboard image. Paste previously routed the clipboard through a device-dependent bitmap and a temporary file written to the working directory, which the packaged Microsoft Store build could not reliably write, so a stale or unrelated image could appear instead; it now reads the clipboard's device-independent bitmap and writes a verified temporary file before opening it.

### Packaging
- Taskbar and Start icons no longer show a square plate behind the rounded Q1View icon. The Microsoft Store package now ships unplated target-size logo assets, so Windows renders the transparent rounded icon directly instead of compositing it onto a square background.

---

## [2.4.0] — 2026-06-13

### Added
- Viewer: press `D` to cycle through the preset resolutions for raw input, mirroring how `N` advances the color space. The shortcut is ignored for sources with an intrinsic size (decoded images and video).

### Fixed
- Viewer: stepping through a folder with PgUp/PgDn (and Home/End) no longer resizes the window around each image. The window keeps its size and the next image is fit into the existing viewport, so browsing mixed-resolution folders stays steady instead of jumping between sizes and in/out of a maximized frame. A fresh open still sizes the window to the image. Fit now shrinks large images to fit but never upscales past 1:1, so small images stay crisp at 100%.

### Packaging
- Refreshed the Viewer and Comparator application icons and the Microsoft Store tile and logo assets, regenerated per size from vector masters for sharper, more consistent edges at every scale.

### Developer
- Qt viewer: a `--selftest` mode plus committed raw-format fixtures and a headless smoke check. `viewer-qt.yml` now runs `ctest` after the build, opening each fixture (yuv420, nv12, nv21, yuv420p10le, p010, grayscale, rgb888, bgr888, rgba8888, bgr565) under the offscreen QPA platform on Linux/macOS/Windows.
- Documented the experimental Qt viewer build and its `qtmultimedia` requirement in the development guide.

---

## [2.3.1] — 2026-06-05

### Fixed
- Comparator: LPIPS model downloads now honor the current user's Windows proxy, PAC, and auto-detect settings in addition to the WinHTTP default proxy.

---

## [2.3.0] — 2026-06-03

### Added
- Comparator: added LPIPS as a perceptual image-quality metric backed by an ONNX Runtime model. The LPIPS model is downloaded on demand, verified with SHA-256, cached under the user's local Q1View data directory, and computed lazily so metric switching stays responsive.
- Viewer: the bottom progress bar now shows playback time alongside the frame counter when a valid timing FPS is available. The readout uses a compact one-decimal format such as `00:01.2 / 00:10.0` and falls back to the existing frame-only display when timing would be misleading.

### Fixed
- Comparator: opening two files from the same image sequence in a single multi-file operation now treats them as explicit single-image panes instead of comparing different offsets of the same discovered sequence.

### Developer
- Visual Studio project files now target the MSVC v143 toolset by default.
- Added ONNX Runtime dependency provisioning for LPIPS builds.

---

## [2.2.0] — 2026-06-02

### Added
- Viewer: thumbnail explorer drawer. Hidden by default, so a fresh launch looks and sizes exactly like before. Open it with File ▸ Thumbnail Browser or the E key: the window slides open to the right (the image stays perfectly still) and slides closed again. The drawer is a compact explorer for the current folder: folders and a ".." entry are shown as full-width text (double-click to browse; at a drive root ".." opens the drive list, so any directory is reachable), image files show a small thumbnail decoded in the background, and raw files (.yuv/.rgb) show an extension badge. Double-click (or Enter) opens an image in the main view; the active file is highlighted and hovering a row shows its full name as a tooltip. Its visibility is remembered between sessions.
- Viewer: Microsoft Store builds check for available Store updates in the background and expose an Update command when a new package is available.

### Fixed
- Viewer and Comparator: smoother mouse-wheel zooming.

### Developer
- Added LPIPS ONNX export tooling.
- Added cross-platform planning notes for Linux and macOS.
- Release tags now submit the Store MSIX to Microsoft Partner Center automatically after the GitHub release build succeeds.

---

## [2.1.0] — 2026-05-30

### Added
- Viewer: drop two or more files to compare them. Dropping multiple files onto Viewer now opens them side by side in Comparator; a single dropped file still opens in Viewer as before.
- File associations: Viewer registers as a handler for the image and raw formats it supports (.yuv, .rgb, .heic, .heif, .hif, .avif, .bmp, .png, .webp, .jpg, .jpeg, .tif, .tiff), so supported files can be opened by double-clicking or through "Open with". Registered in both the MSIX package and the Inno Setup installer.

### Fixed
- Viewer and Comparator normalize forward slashes in paths to backslashes, so sources given with mixed path separators open reliably.

### Packaging
- The MSIX manifest now declares the Visual C++ 2015-2022 runtime as a package dependency, so clean machines no longer fail to launch with "MSVCP140.dll was not found" — the Microsoft Store installs the runtime automatically. The requirement is also disclosed in the Store listing.
- Refreshed the Microsoft Store tile and logo assets from a clean vector master for a sharper, more cohesive icon at every size.

---

## [2.0.0] — 2026-05-29

### Added
- Comparator: pixel-level pink diff overlay. A translucent pink grid plus a center dot highlights every cell that contains at least one pixel that differs from the reference pane. Cell size is fixed in display pixels; zooming implicitly subdivides each cell until each dot resolves to a single differing source pixel at maximum zoom. The overlay hides automatically once zoom is high enough that the per-pixel value labels already convey the diff. Toggle with `D`.
- Comparator: per-pane cursor coordinate readout (`C` to toggle). The hovered pane reports its own source-pixel coordinate; in "Allow Different Resolutions" mode the other panes report the corresponding source-pixel position in their own image dimensions.
- Comparator: click the PSNR / SSIM graph at the bottom to seek to that frame. The graph also zooms with the mouse wheel so individual frame markers stay visible on long sequences with hundreds of frames.
- Comparator: current-frame highlight in pane-specific row colors on the left timeline column (left pane accent blue, right pane warning amber) so left vs right is obvious at a glance.
- Comparator: a per-pane close (X) button on each controls strip with hover highlight. Clicking releases just that pane's source while leaving the slot in place so a new file can be dropped onto it without changing the pane count.
- Comparator: image sequence support. Opening a still image now treats the rest of the folder's same-resolution images as a frame sequence — the timeline scales to the sequence length, Left/Right step frames, and Space plays through the sequence.
- Comparator: playback shortcuts (`Space`, `Left`, `Right`) now work from the timeline column and the bottom metric graph as well, not just the image canvases, so a click into either timeline view keeps keyboard control there.
- Comparator: multi-file open. Drop two-to-four files at once, or pass multiple file paths on the command line, to populate the panes in a single step. Several startup-timing fixes make sure command-line and drag-drop opens land consistently in the intended pane.
- Comparator: panes auto-refresh when their source files are replaced or re-saved on disk — no need to drop the file again.
- Viewer: audio playback for video sources via XAudio2. The controls strip gains a volume slider and a mute button; the audio player closes cleanly when loading a new source.

### Changed
- Comparator: PSNR and SSIM kernels are dramatically faster. PSNR is now vectorized with SSE2 (~6–8× on the Y plane). SSIM is reformulated from the naive O(W·H·64) sliding-window scan to an O(W·H) integral-image box filter; 4K SSIM dropped from hundreds of milliseconds to single-digit milliseconds. The on-UI-thread metric call (file load, on-disk reload, timeline-click seek) no longer produces a perceptible hitch, and `FileScanThread` populates the per-frame metric graph far sooner. PSNR values are bit-exact with the previous scalar code; SSIM values may differ in the last few ULPs of double rounding.
- Comparator: smoother drag / pan. The off-screen `CDC` / `CBitmap` and the pink-overlay GDI+ `Pen` / `SolidBrush` are now cached on the view instead of recreated each paint; at extreme zoom the canvas-sized RGB buffer + `SetDIBitsToDevice` blit is bypassed entirely and each visible source pixel is drawn directly with `FillSolidRect`; the pixel-label `CFont` is cached per zoom level. Rendered pixels are identical to the previous path.
- Internal rename: `Comparer` → `Comparator` across the directory, project / solution files, classes (`CComparerDoc`, `CComparerView`, `ComparerPane`, …), resources, and `Comparator.exe` build output. The user-facing strings were already switched in 05af565; this completes the rename for internal identifiers. The MFC file-type ID `Comparer.Document` is preserved so file associations registered by older installs do not orphan.
- User-facing wording polish: "Custom Fps" → "Custom FPS", "Allow Different Resolution" → "Allow Different Resolutions", "Open a source into a pane" → "Open a source in a pane", "Pick a video frame" → "Seek to a video frame", "Drop a frame source" → "Drop an image or video", "Metric points will appear as frames are parsed" → "Per-frame measurements will appear as frames are processed", "Capture viewer screen or selected region" → "Capture view or selected region" (in Viewer's help), file description "Comparer for all image formats" → "Compare images and video frames".
- README now leads with the target audience and concrete workflows (codec / imaging / CV / QA / research) instead of a generic feature list.
- User guide documents the three secondary panes in Comparator — the left position timeline, the per-frame metric readout, and the bottom metric graph — including their click and playback behavior.

### Fixed
- MSVC C4533 / C4819 warnings in the Comparator and SMultithreadPlus sources (named `cursorPoint` initialization crossed by `goto`; missing UTF-8 BOM on files with non-ASCII characters under code page 949).

### Compatibility note
- Existing installs that ran the previous installer have a leftover `Comparer.exe` in the install directory. The new installer only writes `Comparator.exe`; the orphan file is harmless but stays until manual cleanup or reinstall. A future installer revision can add an explicit cleanup step. Microsoft Store users are unaffected — the Store package only ships `Comparator.exe`.

---

## [1.0.16] — 2026-05-25

### Added
- Viewer shows source-native Y, U, V pixel values when inspecting raw YUV sources at high zoom. Press `V` to toggle between source YUV and display RGB. A corner badge identifies the active mode.

### Changed
- Pixel value component labels clarified to match the active display mode.

---

## [1.0.15] — 2026-05-25

### Added
- Raw YUV formats now support odd-width sources (e.g. 321×241). Previously only even widths were accepted.

### Changed
- Documentation and screenshots refreshed to match the current UI.

---

## [1.0.14] — 2026-05-24

### Changed
- Viewer video playback now follows a `QueryPerformanceCounter`-based presentation clock instead of a self-adjusting timer interval. Playback stays aligned with the source or user-selected FPS and recovers gracefully after device or UI stalls by dropping late frames.
- Playback timer is now cancelled synchronously on close to prevent use-after-free during shutdown.

---

## [1.0.13] — 2026-05-24

### Added
- Viewer context menu shows a check mark next to **Sync Input** when synchronization is active.

### Changed
- Sync Input pan operations are now coalesced before broadcast, reducing latency when panning quickly across multiple linked Viewer windows.

---

## [1.0.12] — 2026-05-24

### Added
- **Sync Input**: opt-in toggle in the Viewer right-click menu that synchronizes navigation, zoom, pan, rotation, playback, FPS, and display mode across all Viewer windows that have it enabled.

### Fixed
- Comparer now resets zoom and pan to a fitted, centered view when opening a new comparison set or when a replacement changes the working resolution. Zoom and pan are preserved when replacing a pane with a source of the same resolution.
- Comparer launched from Viewer now correctly opens the source file.
- Metric average labels no longer overflow their column at high DPI.

---

## [1.0.11] — 2026-05-24

### Fixed
- Video files on Unicode (including Korean) paths now open correctly in Viewer. OpenCV's `VideoCapture` is called through a temporary ASCII-safe copy path to work around its lack of Unicode support.

### Changed
- Legacy in-source TODO lists migrated to tracked GitHub Issues.
- Installer uninstall now cleans up residual files from the install directory.

---

## [1.0.10] — 2026-05-24

### Added
- Windows installer (`Q1ViewSetup-x64.exe`) built and signed by a GitHub Actions pipeline using Inno Setup. The installer registers separate Start Menu entries for Viewer and Comparer.
- AVIF / AV1 still image support documented; HEIF dependency archive updated to include the AOM decoder.

---

## [1.0.9] — 2026-05-24

### Added
- HEIF dependency archive rebuilt with the `aom` vcpkg feature enabled, adding AV1-backed AVIF decoding support.

### Changed
- The x265 HEVC encoder DLL is excluded from the dependency archive, reducing package size and removing an unused component.

---

[Unreleased]: https://github.com/chammoru/Q1View/compare/v2.6.0...HEAD
[2.6.0]: https://github.com/chammoru/Q1View/compare/v2.5.0...v2.6.0
[2.5.0]: https://github.com/chammoru/Q1View/compare/v2.4.4...v2.5.0
[2.4.4]: https://github.com/chammoru/Q1View/compare/v2.4.3...v2.4.4
[2.4.3]: https://github.com/chammoru/Q1View/compare/v2.4.2...v2.4.3
[2.4.2]: https://github.com/chammoru/Q1View/compare/v2.4.1...v2.4.2
[2.4.1]: https://github.com/chammoru/Q1View/compare/v2.4.0...v2.4.1
[2.4.0]: https://github.com/chammoru/Q1View/compare/v2.3.1...v2.4.0
[2.3.1]: https://github.com/chammoru/Q1View/compare/v2.3.0...v2.3.1
[2.3.0]: https://github.com/chammoru/Q1View/compare/v2.2.0...v2.3.0
[2.2.0]: https://github.com/chammoru/Q1View/compare/v2.1.0...v2.2.0
[2.1.0]: https://github.com/chammoru/Q1View/compare/v2.0.0...v2.1.0
[2.0.0]: https://github.com/chammoru/Q1View/compare/v1.0.18...v2.0.0
[1.0.18]: https://github.com/chammoru/Q1View/compare/v1.0.16...v1.0.18
[1.0.16]: https://github.com/chammoru/Q1View/compare/v1.0.15...v1.0.16
[1.0.15]: https://github.com/chammoru/Q1View/compare/v1.0.14...v1.0.15
[1.0.14]: https://github.com/chammoru/Q1View/compare/v1.0.13...v1.0.14
[1.0.13]: https://github.com/chammoru/Q1View/compare/v1.0.12...v1.0.13
[1.0.12]: https://github.com/chammoru/Q1View/compare/v1.0.11...v1.0.12
[1.0.11]: https://github.com/chammoru/Q1View/compare/v1.0.10...v1.0.11
[1.0.10]: https://github.com/chammoru/Q1View/compare/v1.0.9...v1.0.10
[1.0.9]: https://github.com/chammoru/Q1View/releases/tag/v1.0.9
