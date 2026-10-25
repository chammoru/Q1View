# Issue #122 — stage A: Gallery folder-caption alignment

## Scope and review boundary — 2026-10-25

The user authorized proceeding with the reduced [issue #122](https://github.com/chammoru/Q1View/issues/122) design. Work starts from current `origin/master` `e2802de4c894e62140fb378e626473a9f299f0f3` on `codex/issue-122-gallery-alignment`. The design's sequence requires a native visual review of A before the separate UIA implementation B. This change is **A only**, plus its targeted rendering tests and existing Viewer regressions. UIA providers/Narrator acceptance, the remainder of C, merge and release are not marked complete or authorized by this step.

## Implementation

- Ordinary folders, empty folders and drive captions use leading/near alignment on individual DirectWrite layouts. Only parent `[..]` remains horizontally/vertically centered. The shared centered format and extension-badge format remain unchanged.
- GDI fallback uses a dedicated caption helper with top/left wrapping for ordinary folders and single-line centering for the parent. Existing font, card size, proportional/minimum inset, colors, selection, path header, navigation and decoder/cache algorithms are unchanged.
- DirectWrite uses emergency word wrapping so an overlong word is broken before trimming, rather than ellipsizing an intermediate line on narrow cards. Normal word boundaries remain preferred. This edge case was exposed by the new literal-bracket/ampersand fixture, not by the original long-English regression.
- Complete line heights are measured from DirectWrite's actual metrics, and the layout/clip area excludes a partial final row. GDI uses font metrics plus `DT_EDITCONTROL` and a whole-row text rectangle. If no complete row fits, partial glyphs are not painted; tooltip text and native List remain available. This is not a claim that the still-pending Gallery UIA provider already supplies full names.
- DirectWrite may retain zero-height metrics for clipped logical lines. Tests identify the final **visible** line, requiring its trimming when logical text remains, instead of assuming `GetLineMetrics().back()` is visible. Literal strings are never modified or manually split at UTF-16 offsets.

## Targeted verification

- Caption-only actual Viewer run: **4,742 PASS**, exit 0; `Tests/bin/x64/Release/issue-122-captions.txt`.
- The test uses the actual DirectWrite factory and bundled font collection. Its matrix covers 96/120/144/192 DPI inputs, 100/150/200 percent text-scale inputs, 210/360-DIP drawer widths, all five Gallery column counts, and seven short/Korean/English/unbroken/mixed/emoji-combining/literal-symbol captions. It checks per-layout top/left alignment, complete visible rows, final-visible-line-only trimming, original font size and an unchanged shared format. Parent alignment is checked separately.
- Native GDI caption drawing is rasterized into an isolated DIB using those four DPI and three text-scale font inputs, all seven names and centered-parent cases, at a fixed 220-pixel text width and a deliberately fractional-row height. Pixel bounds confirm top/left ordinary captions, centered parent captions, no drawing outside the text rectangle, no partial final row and unchanged Unicode strings. This GDI bitmap matrix is not claimed to cover every real card-width/OS setting combination.
- CoreRegressionTests and GalleryLayoutTests passed; the layout test retained bounded work for both 1,000 and 1,000,000 entries. WindowsUiTypographyTests passed (GDI 55 before / 55 after).
- Instrumented Release/x64/v143 Viewer build passed. The ordinary standard-path Release build also passed; the final executable is `Viewer/x64/Release/Viewer.exe`.

## Regression findings and pending evidence

The first full run reached the existing theme/canvas check, where the fixed 80 ms pump had not produced the matching canvas pixels. That check now awaits the actual expected BGR background with the suite's existing bounded timeout, without forcing a repaint or changing production theme handling. Source, capture, ROI, zoom, selection and cache assertions remain intact. The final full rerun and ordinary-window observation are recorded below after completion; the earlier failed run is not counted as passing.

No Windows DPI, text-size or high-contrast setting has been changed. Font/DPI parameter matrices are not actual OS scaling acceptance. Gallery per-item UIA, Narrator verification, the remaining C combinations and user visual acceptance of A are still pending. The local version remains `0.0.0-dev`; unrelated untracked design files are preserved.

## Final A evidence and native hand-off

- Final full actual Viewer integration: **6,537 PASS**, exit 0; `Tests/bin/x64/Release/issue-122-gallery.txt`. Includes the new caption matrix and existing source, zoom/pan/ROI, themes, input, selection/recycle/playback, directory/watcher, thumbnail and forced GDI/device-recreation regressions. The new caption-only result remains 4,742 PASS. No assertions were waived; the final full run followed the bounded asynchronous-paint test correction.
- Final ordinary Viewer Release/x64/v143 build passed through the standard solution output path; `Tests/bin/x64/Release/issue-122-normal-build.log`. An earlier alternate-output experiment produced PDB type-record warnings from reused intermediate files; it is not the binary used for review. The standard-path build and final relink completed without those warnings.
- Computer Use inspected the ordinary `Viewer/x64/Release/Viewer.exe` in the current Dark theme without changing Windows display/text settings. Repository-owned preview fixtures under `Tests/bin/x64/Release/issue-122-preview` show short, long Korean, long English, unbroken Latin/digits, mixed-script and literal-bracket/ampersand names. The native three-column Gallery visibly keeps [..] centered while ordinary folder names start at the top left and wrap to a final ellipsis. The repository sample remains loaded at Auto / 100.0%.
- To open the ordinary app directly in this review size, only its saved Drawer/ThumbStep was temporarily changed from 0 to 3 before launch and immediately restored to 0 in a finally block. No other app/OS settings were changed. The app is left open in the review Gallery; the generated preview folders are ignored output, not user photographs.

The native Open dialog initially reported stale search-field focus despite the visible filename caret. The actual filename field was selected from the observed tree and its value was set directly; the visible repository preview path was verified before clicking Open. A source-open window-bounds change required refreshing the returned window before retrying Browse. These were automation observations, not new application input changes.

A is ready for the user's visual review. B (Gallery UIA) is deliberately not included or claimed complete, and #122 remains open. No merge, tag, version bump or Store submission is part of this hand-off.
