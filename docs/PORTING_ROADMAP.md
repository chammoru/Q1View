# Q1View Cross-Platform Port — Development Roadmap

This document is the forward-looking companion to
[PORTING_STATUS.md](PORTING_STATUS.md). `PORTING_STATUS.md` records *what
already works* on `master`; this file describes *what is planned
next, in what order, and why* — the path from the current minimal Qt viewer to
parity with the Windows-only `Viewer` and `Comparer` apps on master.

> This is a planning document, not a contract. Phases can be reordered when
> evidence supports it, but the dependencies called out under each phase
> should be respected.

## Table of contents

- [Goal](#goal)
- [Non-goals](#non-goals)
- [Guiding principles](#guiding-principles)
- [Current baseline](#current-baseline)
- [Phase 0 — Foundation hardening](#phase-0--foundation-hardening)
- [Phase 1 — Viewer interaction parity](#phase-1--viewer-interaction-parity)
- [Phase 2 — Video and audio](#phase-2--video-and-audio)
- [Phase 3 — Multi-window Sync Input](#phase-3--multi-window-sync-input)
- [Phase 4 — Distribution maturity](#phase-4--distribution-maturity)
- [Phase 5 — ComparatorQt](#phase-5--comparatorqt)
- [Phase 6 — Polish, docs, parity tracking](#phase-6--polish-docs-parity-tracking)
- [Ordering rationale](#ordering-rationale)
- [Tracking master changes](#tracking-master-changes)
- [Open questions](#open-questions)

## Goal

Ship a single Qt-based codebase that runs on Windows, macOS, and Linux and
covers the feature set of:

- The MFC [Viewer/](../Viewer/) app on master (image / raw / sequence / video
  inspection with sync between windows).
- The MFC [Comparer/](../Comparer/) app on this branch — renamed to
  `Comparator/` on master in
  [#43](https://github.com/chammoru/Q1View/pull/43). Both names refer to the
  same product; this document calls the Qt port **ComparatorQt** to match
  the post-rebase name.

The MFC apps on `master` remain the Windows production reference until the
Qt port reaches parity *and* its distribution channels are equally trusted.

## Non-goals

- Rewriting the Windows MFC apps or removing them.
- Diverging behavior from the Windows reference without a stated reason.
  Any intentional deviation must be recorded in `PORTING_STATUS.md`.
- Adding new user-visible features that the Windows apps do not have, until
  parity is reached. New features land on master first, then port across.

## Guiding principles

- **Portable core stays UI-agnostic.** `q1view_image_core`, `qimage_cs`, and
  `SMultithreadPlus` must not pull in Qt. Build them as plain CMake static
  libraries usable by both the Qt apps and (eventually) any other frontend.
- **Share between ViewerQt and ComparatorQt.** As ComparatorQt arrives,
  extract reusable widgets — zoom canvas, pixel-text overlay, raw-open
  dialog, metric graph — into a `viewer_qt_common` static library so the
  two apps stay consistent and do not drift over time.
- **Each phase ships a prerelease.** Continue the `qt-viewer-v*` tag scheme
  established with
  [`qt-viewer-v0.2.0`](https://github.com/chammoru/Q1View/releases/tag/qt-viewer-v0.2.0).
  Cutting a tag at the end of each phase keeps feedback continuous and
  makes regressions traceable to a specific phase.
- **Master is the source of truth for behavior.** When porting a feature,
  read the corresponding code under [Viewer/](../Viewer/) or
  [Comparer/](../Comparer/) and the matching section of
  [USER_GUIDE.md](USER_GUIDE.md) before designing the Qt version.
- **Avoid premature widget abstractions.** The first port should be a
  direct translation. Only extract into `viewer_qt_common` after the second
  use site appears (ComparatorQt). The current
  [ViewerQt/MainWindow.cpp](../ViewerQt/MainWindow.cpp) (~1100 lines) will
  also need a refactor pass when this happens, but that work belongs to the
  phase that triggers the extraction, not to a speculative cleanup.

## Current baseline

From [PORTING_STATUS.md](PORTING_STATUS.md) — the Qt viewer today already
covers:

- Open common image formats via `QImageReader` (PNG / JPG / BMP).
- Open raw frames with width / height / color space, with persistence.
- Drag-and-drop, CLI raw flags.
- Wheel zoom, drag pan, actual-size / fit / fullscreen / rotate clockwise.
- Y-only toggle, cursor coordinate + RGB display.
- Copy / paste / save-as / close.
- Previous / next file navigation.
- Raw multi-frame navigation and timer playback.
- Linux AppImage packaging; Windows and macOS Qt deploy artifacts;
  prerelease workflow under `qt-viewer-v*`.

The remaining gap, grouped:

**Viewer features missing on Qt**

- Video file playback (OpenCV / FFmpeg) with audio.
- HEIF / HEIC / AVIF still-image decoding.
- Selection mode (`S`) and `Ctrl+C` capture of the selected region.
- High-zoom per-pixel text overlay (the "read every pixel value" mode).
- Source-YUV vs RGB pixel value toggle (`V`), hex / dec toggle (`H`).
- Color-space cycle (`N`), pixel interpolation toggle (`I`), box info (`B`).
- Built-in shortcut help panel (`?`).
- Custom FPS dialog ([CustomFpsDlg](../Viewer/CustomFpsDlg.cpp)).
- Recent files (MRU).
- Volume slider, mute button, seek band.
- File-change auto-refresh
  ([FileChangeNotiThread](../Viewer/FileChangeNotiThread.cpp)).
- Multi-window **Sync Input**.

**Comparer entirely unported**

- 2–4 pane comparison.
- PSNR and SSIM metrics ([MetricCal](../Comparer/MetricCal.cpp)).
- Pink-dot pixel diff overlay ([QImageDiff](../Comparer/QImageDiff.cpp)).
- Position timeline pane.
- Per-frame metric label.
- Metric-over-time graph.
- Allow-different-resolution mode.

---

## Phase 0 — Foundation hardening

**Why now:** every later phase depends on a decoder layer that is not
`QImageReader`-only, and on a file scanner / watcher that does not rely on
Windows-specific code. Doing both once avoids re-doing them inside Phases 1,
2, and 5.

**Outcome:** ViewerQt can open every still-image format master supports,
detect external changes to the current file, and enumerate sibling files
through the same code path that ComparatorQt will later reuse.

### Workstreams

#### 0.1 Decoder abstraction (`IFrameSource`)

Mirror the Windows shape in [FrmSrc.h](../Viewer/FrmSrc.h),
[MatFrmSrc.h](../Viewer/MatFrmSrc.h), [RawFrmSrc.h](../Viewer/RawFrmSrc.h),
[VidCapFrmSrc.h](../Viewer/VidCapFrmSrc.h) but expose a Qt-friendly
interface that returns `QImage` or a raw buffer + format descriptor.

```text
class IFrameSource {
public:
    virtual ~IFrameSource() = default;
    virtual int frameCount() const = 0;
    virtual QImage frameAt(int index) = 0;
    virtual bool hasAudio() const { return false; }
    // Optional: float fps() const; QSize sourceSize() const;
};
```

Concrete backends, each guarded by a CMake option:

| Backend         | Covers                                  | Dependency       |
| --------------- | --------------------------------------- | ---------------- |
| `QImageReader`  | PNG / JPG / BMP / TIFF / WebP           | Qt only          |
| `RawFrameSource`| Existing raw flow                       | `qimage_cs`      |
| `HeifSource`    | HEIF / HEIC / HIF                       | libheif          |
| `AvifSource`    | AVIF / AV1 still                        | libavif          |
| `VideoSource`   | Video files                             | OpenCV + FFmpeg  |

`VideoSource` only stubs its constructor in Phase 0; it is wired up in
Phase 2. The point of declaring it here is to prove the interface accepts
multi-frame, audio-bearing sources without later refactor.

#### 0.2 Sibling-file scanner

Port [FileFinderThread.cpp](../Viewer/FileFinderThread.cpp) to a
`QThread`-based class that emits `QStringList` of files in the current
directory matching any registered backend's filter. ViewerQt's existing
prev/next navigation in [MainWindow.cpp](../ViewerQt/MainWindow.cpp) (see
`openAdjacentFile`) currently relies on `QDir::entryList` synchronously —
swap it for the threaded scanner once results land asynchronously.

#### 0.3 File-change watcher

Replace [FileChangeNotiThread.cpp](../Viewer/FileChangeNotiThread.cpp) with
`QFileSystemWatcher`. The Windows version fires when the open file changes
on disk; the Qt version should do the same, plus debounce rapid back-to-back
writes (editors that write-then-rename trigger two events within ms).

### Implementation notes

- The decoder factory should dispatch by extension first, falling back to
  magic-byte sniffing for files without a recognized extension (matches the
  Windows behavior for clipboard/dropped content).
- Keep all decoder backends in a new `ViewerQt/decoders/` subdirectory and
  add them to `CMakeLists.txt` only when their dependency is found. CI
  builds with `Q1VIEW_WITH_HEIF=ON`, `Q1VIEW_WITH_AVIF=ON`,
  `Q1VIEW_WITH_VIDEO=ON`; user builds may opt out.
- The watcher should also re-emit on file deletion so the Viewer can show a
  "file removed" state instead of a stale frame.

### Test plan

- Extend [tests/core_smoke.cpp](../tests/core_smoke.cpp) to round-trip one
  HEIF and one AVIF sample (gated on backend availability).
- Add a smoke test for the sibling scanner using a temp directory.
- Manual: open the same file in two ViewerQt windows; modify it on disk
  with an editor; both should refresh.

### Dependencies

- **Blocks:** Phases 1, 2, 5.
- **Blocked by:** nothing.

### Risks

- HEIF / AVIF licensing for the macOS App Store — flag for legal review
  *before* Phase 4 ships an unsigned macOS bundle that hits the user.

### Effort and exit

- Effort: ~1.5–2 weeks for one engineer.
- Exit: prerelease `qt-viewer-v0.3.0-alpha` with HEIF / AVIF support,
  threaded scanner, and auto-refresh, on all three OSes in CI.

---

## Phase 1 — Viewer interaction parity

**Why now:** the Qt viewer is already minimally usable. Closing the
interaction gaps turns it from "demo" into "I would actually use this for
image work" *before* spending weeks on video / audio infrastructure that
benefits a smaller fraction of sessions.

**Outcome:** every shortcut listed under the *Viewer Controls* table in
[USER_GUIDE.md](USER_GUIDE.md) behaves the same as the MFC viewer on
Windows, macOS, and Linux.

### Workstreams, in priority order

Each subsection below corresponds to one focused PR. The ordering is
*highest user-visible value per LOC first*.

#### 1.1 Built-in shortcut panel (`?`)

Cheap and disproportionately useful. The MFC version is a modeless help
overlay populated from a static table. Implement as a `QDialog` (non-modal)
fed by a shared header table, so ComparatorQt can reuse the same shape with
a different table in Phase 5.

#### 1.2 Hex / dec pixel value toggle (`H`)

[ViewerView.cpp](../Viewer/ViewerView.cpp) tracks `mHexMode` and swaps the
RGB format string. ViewerQt already has the RGB readout next to the cursor
coordinate — add an action and a format switch.

#### 1.3 High-zoom per-pixel text overlay

The "read every pixel" mode. In the MFC view it overlays a hex or dec
triplet on each source pixel once the zoom is high enough that the cell
fits the text. Port the existing logic — including the cached font in
`mDefPixelTextFont` and the box detection from the `Comparer: skip
canvas-sized DIB at extreme zoom; cache pixel-text font` change on master
(commit `8641818`) — directly into a custom `QGraphicsItem` or paint pass
on the canvas widget.

#### 1.4 Source-YUV vs RGB pixel value toggle (`V`)

When the source is YUV-derived, holding `V` swaps between displayed RGB and
the original YUV triplet for the cursor pixel. ViewerQt currently always
shows RGB; expose a `QAction` and have the raw decoder backend cache the
last-decoded source pixel plane so the lookup is constant-time.

#### 1.5 Cursor coord toggle (`C`), pixel interpolation toggle (`I`), box info (`B`)

Three more keyboard toggles. All three are state on the view; none need
new infrastructure.

#### 1.6 Color-space cycle (`N`)

Cycle through supported color spaces in `qimage_cs` for the current source.
The list is the same as the raw-open dialog's color-space combo.

#### 1.7 Selection mode (`S`) + region capture

In the MFC viewer, `S` enters a selection mode where left-drag draws a
rectangle on the canvas; `Ctrl+C` then copies *that region only* instead
of the whole image. Port:

- The selection state lives on the view, not the document.
- The selection rectangle clamps to the source image bounds, not the
  zoomed canvas.
- Capture path: convert the canvas rect to source coordinates, then
  `QImage::copy(sourceRect)` and push to the clipboard.

#### 1.8 Recent files (MRU)

`QSettings`-backed list with default size 8. Surface as a dynamic submenu
under `File`. Distinguish "raw with metadata" entries from plain image
files so re-opening a raw file does not need to re-prompt for dimensions.

#### 1.9 File-change auto-refresh

Wire the watcher from §0.3 into the open document. Reload the current
frame on change; preserve zoom, pan, and selection (do not snap back to
fit).

#### 1.10 Custom FPS dialog

Straight port of [CustomFpsDlg.cpp](../Viewer/CustomFpsDlg.cpp) using
`QInputDialog::getDouble`. Persists per-session, not globally (matches
Windows).

### Dependencies

- **Blocked by:** none for §1.1–§1.6, §1.8, §1.10; §1.9 blocked by §0.3.
- **Blocks:** Phase 6 audit.

### Risks

- The pixel-text overlay is the only item likely to take longer than its
  estimate. The MFC implementation is bit-twiddly. Time-box at 3 days; if
  it grows, split the precise per-cell text positioning into a follow-up
  issue and ship the rest of Phase 1 without it.

### Effort and exit

- Effort: ~2–3 weeks for one engineer.
- Exit: prerelease `qt-viewer-v0.4.0` with the §1.1–§1.10 items, side-by-side
  tested against the MFC viewer for shortcut behavior.

---

## Phase 2 — Video and audio

**Why now:** video is the single largest unaddressed input class. It also
requires picking a cross-platform audio backend — a non-trivial commitment
that benefits from a stable interaction layer underneath. Doing it before
Phase 3 (Sync Input) avoids retrofitting playback events into the sync
protocol.

**Outcome:** an MP4 with audio plays back on all three OSes with seek,
mute, and volume controls comparable to master's Windows viewer.

### Workstreams

#### 2.1 Video frame source

Implement `VideoSource` (stubbed in §0.1) on top of OpenCV's `VideoCapture`
with the FFmpeg backend already bundled in the master release. Keep frame
decode off the UI thread:

- Background `QThread` owns the `VideoCapture`.
- Decoded frames flow through a bounded `QQueue<QImage>` to the UI.
- Seek is a synchronous request on the background thread (drains the queue
  first to avoid showing stale frames).

#### 2.2 Audio backend

Recommendation: **QtMultimedia `QAudioSink`**. One backend everywhere, no
per-OS code paths. Fallback if QtMultimedia proves inadequate (high
latency on seek, in particular):

| OS      | Native backend |
| ------- | -------------- |
| Windows | WASAPI         |
| macOS   | Core Audio     |
| Linux   | PulseAudio (or PipeWire passthrough) |

The Windows MFC viewer uses XAudio2 via
[AudioPlayer](../Viewer/AudioPlayer.cpp); the per-OS fallback list above
matches what each platform would idiomatically use.

#### 2.3 Playback controls UI

Port from the Windows viewer:

- Volume slider and mute button — see the `Viewer: volume slider, mute
  button, and seek-range fix` change on master.
- Seek band with click-to-seek.
- A/V sync logic — keep the existing video frame clock as the master; pull
  audio frames forward to it on drift.

Unify the playback clock with the raw multi-frame timer already in
ViewerQt so there is one play / pause / step path.

#### 2.4 Decoder packaging

Bundle FFmpeg dynamic libraries:

- **Windows:** copy into the `windeployqt` output.
- **macOS:** `dylib` into the `.app` Frameworks/; rewrite `install_name`
  for Gatekeeper.
- **Linux:** rely on the AppImage's `linuxdeploy-plugin-qt` for now; add
  an FFmpeg plugin when missing.

### Test plan

- MP4 / MOV / MKV samples, each on each OS, validated against master's
  Windows viewer on the same files.
- Mute and volume state persist via `QSettings`.
- A/V drift budget: ≤ 80 ms peak, ≤ 40 ms sustained across a 5-minute
  clip. Define and measure during the phase; tighten if QtMultimedia
  outperforms the budget.
- Seek to N random timestamps; first frame after seek must show within
  300 ms.

### Dependencies

- **Blocked by:** §0.1 (decoder abstraction must accept multi-frame
  audio-bearing sources without refactor).
- **Blocks:** Phase 3 (sync needs play / pause / seek to be testable).

### Risks

- macOS Gatekeeper / hardened-runtime constraints for FFmpeg dynamic
  libraries — verify *before* the first Phase 2 prerelease, not after
  notarization fails.
- QtMultimedia's seek latency on some Linux distros has historically been
  poor; reserve the per-OS fallback decision until measured.

### Effort and exit

- Effort: ~3–4 weeks for one engineer.
- Exit: prerelease `qt-viewer-v0.5.0` with video + audio playback on all
  three OSes.

---

## Phase 3 — Multi-window Sync Input

**Why now:** ComparatorQt (Phase 5) is one process with multiple panes,
but Viewer's Sync Input is *multi-process* and uses Windows-only
`WM_COPYDATA`. The IPC replacement is small but unique to Viewer; doing
it here keeps Viewer feature-complete before the Comparator effort starts,
and prevents Phase 5 from accidentally absorbing the IPC design work.

**Outcome:** two or more ViewerQt processes mirror navigation, zoom, pan,
rotation, playback, FPS, and display-mode state when **Sync Input** is
enabled, on all three OSes.

### Workstreams

#### 3.1 IPC channel

Replace `WM_COPYDATA` with one of:

- **`QLocalServer` / `QLocalSocket`** — a named pipe / Unix domain socket.
  Best fit for small JSON or `QDataStream` payloads. Recommended.
- `QSharedMemory` + named semaphore — only consider if measurements show
  the socket path adds noticeable latency to fast pan / zoom updates.

The first viewer to launch creates the server under a stable name (e.g.
`q1view-sync-<user>`); subsequent viewers connect as clients. If the
server viewer exits, the next-oldest client promotes itself.

#### 3.2 State protocol

A single struct, serialized via `QDataStream`:

```text
struct SyncMessage {
    quint64 originHwndHash; // ignore own messages
    quint8  kind;           // nav | zoom | pan | rotation | playback | fps | display
    QVariant payload;       // kind-specific
};
```

Mirror exactly the event set the Windows version syncs. Anything beyond
that (e.g. "follow my selection rectangle") is out of scope.

#### 3.3 Sync Input toggle

Context-menu entry in the right-click menu, matching the screenshot in
[USER_GUIDE.md § Synchronized Viewer Windows](USER_GUIDE.md#synchronized-viewer-windows).
Off by default. State persists per-instance via `QSettings`.

### Test plan

- Launch 2 and 4 ViewerQt instances; toggle Sync Input on each; verify
  every synced event class is mirrored.
- Kill the first instance; verify the remaining instances continue to
  sync among themselves.
- Cross-OS: open the same shared folder via Samba on macOS and Windows
  simultaneously, sync between them. (Note: cross-OS sync via the same
  named pipe path is *not* a goal; this test is just to verify each OS
  behaves correctly with its own local instances.)

### Dependencies

- **Blocked by:** Phase 2 — sync needs play / pause / seek state to be
  meaningful.
- **Blocks:** nothing critical, but cleanest to land before Phase 5 so
  Comparator does not import a sync API that has not been used in anger.

### Risks

- Named-pipe permissions on Windows when multiple Windows accounts run
  the same instance — restrict the pipe to the current user's SID.
- macOS / Linux Unix domain socket path length cap (~104 chars) — derive
  the path from `QStandardPaths::TempLocation`, not from the home dir.

### Effort and exit

- Effort: ~1.5–2 weeks for one engineer.
- Exit: prerelease `qt-viewer-v0.6.0` with Sync Input working on all three
  OSes.

---

## Phase 4 — Distribution maturity

**Why now (parallel-able with Phases 1–3):** distribution work blocks on
platform tooling (Apple Developer cert, Flathub review queue) rather than
on engineering inside the codebase. Start long-lead items as soon as Phase 1
has something worth shipping. The phase as listed is mostly about *bringing
each channel to "users install via the OS-native store"* rather than "users
download a zip and unpack it."

**Outcome:** every supported OS has a one-click install and a documented
auto-update path.

### Workstreams

#### 4.1 Linux: Flatpak + AppStream

- Keep the AppImage (already produced by the
  [release-qt-viewer workflow](../.github/workflows/release-qt-viewer.yml)).
- Add a Flatpak manifest under `packaging/flatpak/`.
- Add AppStream metadata
  (`packaging/linux/q1view-viewer-qt.appdata.xml`) so Discover and GNOME
  Software show the app with screenshots, description, release notes.
- Submit to Flathub once Phase 1 has shipped.

#### 4.2 macOS: signed and notarized DMG

- Add `codesign` + `notarytool` steps to
  [release-qt-viewer workflow](../.github/workflows/release-qt-viewer.yml).
- Store the Apple Developer cert in GitHub Actions secrets.
- Hardened runtime entitlements: required for notarization. Audio capture
  is not currently a feature, so no `com.apple.security.device.audio-input`
  needed yet.
- Bundle FFmpeg dylibs with corrected install names (see §2.4).

#### 4.3 Windows: pick one channel

Decide between (a) folding the Qt build into the existing MSIX channel,
or (b) shipping a separate portable `.zip` and installer. Two channels is
acceptable short term but eventually pick one to keep update behavior
consistent for end users.

If joining MSIX: the existing
[AppxManifest.xml](../installer/msix/AppxManifest.xml) ships the MFC viewer
today; either add a second app entry for the Qt viewer or replace the MFC
viewer entry once parity is reached.

#### 4.4 Auto-update story

| OS      | Mechanism                                             |
| ------- | ----------------------------------------------------- |
| Windows | Microsoft Store handles updates when shipped via MSIX (overlaps with master issue #45) |
| macOS   | Sparkle (preferred) or Squirrel                       |
| Linux   | Flathub handles updates for the Flatpak channel; AppImage users self-update |

### Test plan

- Each OS: install via the native channel on a clean machine; open a file;
  uninstall. No terminal commands required.
- Each OS: trigger an update from N → N+1 and verify the app restarts in
  the new version with settings preserved.

### Dependencies

- **Blocked by:** at least one prerelease from Phase 1 (so there is
  something worth installing).
- **Blocks:** nothing internal, but blocks "1.0" announcement.

### Risks

- Apple Developer cert lead time can be weeks; start the application as
  soon as Phase 1 ships.
- Flathub review can also be slow; submit early.

### Effort and exit

- Effort: ~2 weeks of engineer time spread over a calendar-time wait of
  several weeks.
- Exit: documented install + update path on each OS, linked from
  [README.md](../README.md).

---

## Phase 5 — ComparatorQt

**Why last:** ComparatorQt is the largest single chunk of new UI work and
it benefits from a battle-tested ViewerQt plus the abstractions laid down
in Phases 0–2 (decoders, file scanner, watcher, video, audio). Doing it
earlier would mean either duplicating viewer logic or building the
abstractions twice.

**Outcome:** ComparatorQt can open 2–4 sources, report PSNR / SSIM per
frame and over time, draw the pink-dot pixel diff overlay, and let the
user click the timeline or graph to seek — matching master's Comparer
behavior.

### Workstreams, in implementation order

#### 5.1 Extract `q1view_metrics` static lib

Pull metric implementations out of [Comparer/MetricCal.cpp](../Comparer/MetricCal.cpp)
and the per-format strategies
([RgbFrmCmpStrategy.cpp](../Comparer/RgbFrmCmpStrategy.cpp),
[YuvFrmCmpStrategy.cpp](../Comparer/YuvFrmCmpStrategy.cpp),
[FrmCmpStrategy.cpp](../Comparer/FrmCmpStrategy.cpp)) into a
UI-free static library.

**Keep the SSE2 + integral-image fast path** that master added in commit
`4f642be` (`QVisionCore: speed up PSNR/SSIM with SSE2 and integral-image
box filter (#10)`). This branch will inherit that commit on its next
rebase; the extraction must preserve it.

On non-x86 (macOS arm64, Linux arm64), provide a scalar fallback gated by
`__SSE2__`. Add NEON later if profiling demands it; do not block Phase 5
on it.

Unit-test the metrics against fixed reference image pairs whose PSNR /
SSIM is verified once against the master MFC build.

#### 5.2 ComparatorQt skeleton

- `MainWindow` containing a `QSplitter` (horizontal for 2-pane).
- Each pane is a reusable `CanvasWidget` extracted from
  [ViewerQt/MainWindow.cpp](../ViewerQt/MainWindow.cpp) into
  `viewer_qt_common` — this is the trigger to do the long-postponed
  refactor mentioned under [Guiding principles](#guiding-principles).
- Drop targets per pane.
- Paired navigation: left arrow advances both panes; sole pane keeps its
  own controls for testing.

#### 5.3 Pixel-level diff overlay

Port [QImageDiff.cpp](../Comparer/QImageDiff.cpp):

- Fixed-size grid of display-pixel cells.
- Cell highlighted with a translucent pink rectangle + center dot if any
  source pixel within it differs from the reference.
- Hidden automatically when zoom is high enough to show per-pixel values
  (reuse the same threshold as the pixel-text overlay from §1.3).
- Toggle with `D`.

#### 5.4 Position timeline pane

Vertical strip listing every frame in the active pair. Red marker for
differing frames, blank for identical. Click to seek both panes. Routes
`Space`, `Left`, `Right` (master's
[`Comparer: route playback shortcuts through both timeline views`](https://github.com/chammoru/Q1View/commit/0ca4582)
behavior — inherited on next rebase).

#### 5.5 Per-frame metric text

Label between timeline and graph. Shows PSNR or SSIM for the current
frame pair plus channel breakdowns. Updates on frame change or metric
selection change.

#### 5.6 Metric-over-time graph

Plot the metric across every frame. Marker for current frame, running
average overlay, click-to-seek, wheel-zoom horizontal axis, double-click
to reset zoom.

**Do not use `QtCharts`.** It is heavy, depends on the Qt commercial /
GPL split for some setups, and its interaction model fights click-to-seek
and wheel-zoom semantics. Implement as a custom `QGraphicsScene` subclass
or a plain `QWidget` with `paintEvent`. The data volume (one value per
frame, single sequence) is small enough that custom paint is faster than
configuring QtCharts.

#### 5.7 3- and 4-pane layouts

Port [ComparerPane.h](../Comparer/ComparerPane.h) and
[QSplitterWnd.cpp](../Comparer/QSplitterWnd.cpp) layout logic. Use a
single horizontal `QSplitter` for 3-pane and a 2×2 grid (`QSplitter`
containing two horizontal `QSplitter`s) for 4-pane. Per-pane close button
ports from master's
[`Comparer: add per-pane close button with hover state`](https://github.com/chammoru/Q1View/commit/76941cf)
(inherited on next rebase).

#### 5.8 Allow Different Resolution mode

OPTIONS toggle: when on, panes can show sources of different resolutions
in independent canvases; cursor coordinates are mapped per pane (master's
[`Comparer: map cursor coord per pane in different-resolution mode`](https://github.com/chammoru/Q1View/commit/b17a9a8)).

### Test plan

- Numerical: PSNR and SSIM on a fixed reference pair must agree with
  master's Windows Comparer within `1e-6` (relative tolerance — same
  algorithm, same SSE2 path).
- Visual: side-by-side pink-dot overlay against Windows at zoom levels
  100 %, 200 %, 800 %.
- Shortcut audit against
  [USER_GUIDE.md § Comparator Controls](USER_GUIDE.md#controls-1).
- Drop one image + one video; verify per-frame metrics step in lockstep.

### Dependencies

- **Blocked by:** Phases 0, 1, 2.
- **Blocks:** Phase 6 final parity audit.

### Risks

- The `CanvasWidget` extraction from ViewerQt may take longer than
  estimated because `MainWindow.cpp` (~1100 lines) was not written with
  reuse in mind. Time-box the extraction at 1 week; if it overruns,
  copy-paste the canvas code into ComparatorQt and unify later.
- The metric SSE2 path assumes x86. Arm fallback must exist before the
  first macOS arm64 / Linux arm64 prerelease.

### Effort and exit

- Effort: ~4–6 weeks for one engineer.
- Exit: prerelease `qt-comparator-v0.1.0` with §5.1–§5.8.

---

## Phase 6 — Polish, docs, parity tracking

**Why ongoing:** documentation drift is the silent killer of cross-platform
ports. This phase is not a final sprint — it is a habit started in Phase 1
and never stopped.

**Outcome:** a user landing on the README cannot tell from the docs whether
they are reading about the MFC apps or the Qt apps until they want to know.

### Workstreams

- Add a per-OS section to [USER_GUIDE.md](USER_GUIDE.md) for any deltas
  between the Qt port and the MFC reference. Aim for zero deltas; document
  what remains.
- Re-capture every screenshot from the Qt builds on the OS the
  screenshot's audience most likely uses (Windows screenshots for Windows
  users, etc.).
- Keep [PORTING_STATUS.md](PORTING_STATUS.md) updated as the canonical
  "what works today" source. Keep this file (`PORTING_ROADMAP.md`) as the
  "what's next" source.
- Run a parity audit at the end of Phases 1, 3, 5 against the controls
  tables in [USER_GUIDE.md](USER_GUIDE.md). Anything missing becomes a
  tracked issue with the `enhancement` label, linked from the relevant
  Phase's exit note.
- Decommission decision for the MFC apps once Qt parity is reached:
  frozen, deprecated, or removed. Record the decision in
  [PORTING_STATUS.md](PORTING_STATUS.md), not here.

### Effort

- Effort: small, spread across all phases. Budget ~0.5 day per PR that
  touches user-visible behavior.

---

## Ordering rationale

| Phase | Unlocks                                | Blocks                  | Effort¹ |
| ----- | -------------------------------------- | ----------------------- | ------- |
| 0     | Codec / file primitives                | Phases 1, 2, 5          | M       |
| 1     | Daily-driver usability                 | —                       | M       |
| 2     | Video / audio class of input           | Phase 3                 | L       |
| 3     | Multi-window workflow                  | (clean before Phase 5)  | S       |
| 4     | Real distribution                      | (parallel)              | M       |
| 5     | Comparator entirely                    | —                       | L       |
| 6     | Documentation & parity proof           | (parallel, ongoing)     | S       |

¹ S ≈ days, M ≈ 1–2 weeks, L ≈ 3–6 weeks for one engineer. Treat as a
shape estimate, not a commitment.

The single most important sequencing constraint is **Phase 0 before
Phases 1, 2, 5** — the decoder abstraction. Phase 2 before Phase 3 is a
softer constraint (sync benefits from having playback to sync, but is not
strictly blocked). Phase 4 can begin as soon as Phase 1 ships a usable
prerelease. Phase 5 is the only phase that should not start before
everything else is mature; the failure mode of starting it too early is
duplicated effort that gets thrown away when the abstractions land.

## Tracking master changes

> **Historical.** `cross-platform-core` has since been merged into `master` and
> retired (#54); porting work now lands directly on `master`, so there is no
> branch left to rebase. The checklist below is kept for reference only.

Master keeps moving while this branch progresses. At the time of writing
this branch is 42 commits ahead and 85 commits behind master, including
the Comparer→Comparator rename, audio improvements, and the SSE2 PSNR /
SSIM path. Each time `cross-platform-core` is rebased on master:

1. Update the [Current baseline](#current-baseline) section if any
   porting work was made obsolete or easier by an upstream change.
2. Re-evaluate the Phase 5 list for new Comparer / Comparator features.
   For example, master commits since the merge base added:
   - `Comparer: route playback shortcuts through both timeline views` (#11)
   - `Comparer: show source-pixel coords under the cursor` (#9)
   - `Comparer: skip canvas-sized DIB at extreme zoom; cache pixel-text font` (#7)
   - `Comparer: map cursor coord per pane in different-resolution mode`
   - `Comparer: zoom the metric graph with the mouse wheel` (#17)
   - `Comparer: click metric graph to seek to that frame` (#8)
   - `Comparer: pink-dot grid overlay for pixel-level diffs` (#13)
   - `Comparer: auto-refresh panes when source files change on disk` (#18)

   These are now part of the Phase 5 scope, not follow-ups.
3. Re-run the Phase 1 audit against the latest USER_GUIDE.md in case a
   new keyboard toggle has been added on master.
4. Cut a fresh prerelease only after the rebase is green on CI for all
   three OSes.

## Open questions

- **Audio backend:** commit to QtMultimedia, or build per-OS for
  lower-latency seek? Decide during Phase 2 measurement.
- **HEIF / AVIF on macOS App Store:** feasible without bundling our own
  decoder? Verify before Phase 4 macOS path.
- **MFC apps long-term:** frozen, deprecated, or removed once Qt parity
  is reached? Record the decision in `PORTING_STATUS.md` after Phase 5.
- **Decoder selection mechanism:** build-time CMake option (current
  Phase 0 plan) or runtime plugin discovery? Stay with build-time unless
  user demand for hot-swappable decoders appears.
- **`viewer_qt_common` extraction timing:** wait until Phase 5 forces it
  (current plan), or extract early in Phase 1 to keep ViewerQt clean?
  Current plan is "wait" — avoids speculative abstractions — but revisit
  if `ViewerQt/MainWindow.cpp` becomes painful to modify during Phase 1.

Resolve each before the phase it gates begins; record the resolution in
the matching phase note above.
