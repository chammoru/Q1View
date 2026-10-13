# Windows native appearance and DPI acceptance follow-up

Date: 2026-10-14 (Asia/Seoul). Scope: existing Windows Viewer/Comparator work for #118/#139. The user explicitly approved temporarily changing Windows app colors, high contrast, and display scale, then restoring the original settings. No release/merge/version change is authorized by this verification.

## Original active settings and restoration checklist

- Windows Colors: Custom; Windows mode Dark (`SystemUsesLightTheme=0`), app mode Light (`AppsUseLightTheme=1`). Restored after the Light -> Dark -> Light check.
- Windows contrast theme: None (off). Temporarily applied Night sky, then restored None; observed HighContrast Flags 127 while on and 126 after off.
- Display: 1920 x 1080, landscape, scale 100% (recommended). Resolution/orientation are not changed. Scale was restored 100% before editing and again after all retests; final Settings inspection confirms 100%, 1920x1080, landscape.
- Q1View shared Theme: 2 (Dark). Temporarily selected System (0) through Viewer's Options > Theme so both apps follow the actual OS changes; restored Dark (2) through the same menu before working on the regression.
- OS text-size factor is not changed; native test reports show 1.000.

## Ordinary applications and media

Viewer: `Viewer/x64/Issue139ViewerPaintReview/Viewer.exe`, repository `Tests/fixtures/sample_16x16.png`, 1.00x. Comparator: `Comparator/x64/Issue139PairReview/Comparator.exe`, four disposable 640x360 demo videos (A/B/C/D: 150/150/90/180 frames), paused at frame 30, 0.67x, with a synchronized 225x151 ROI. No private images are used.

Before the OS transitions, A/B, A/C and A/D full PSNR scores were 33.9524, 37.4304 and 29.3136 dB; crop scores were 29.5280, 32.5240 and 27.8221. Both sets of scores, source filenames, frame 30 and the ROI remained unchanged after the actual Windows app Light -> Dark -> Light changes and high-contrast on/off. Viewer retained its file, decoded image and 1.00x readout. Native observation is not a byte-level source/capture checksum; those invariants are separately checked by the integration suites.

## Actual OS theme / high-contrast checks

Windows Colors was changed through Settings, not an injected app message. System-theme UI and surround followed in both ordinary applications while media stayed open. Windows mode remained Dark throughout; app mode was restored Light, confirmed through the UI and read-only registry inspection. Q1View's preference remained System during these checks, without restarting either application.

Night sky was applied from Accessibility > Contrast themes. Both ordinary applications switched to native high-contrast caption/menu rendering, system-colored canvas/text, and Comparator's native scrollbar fallback. The ROI border and channel graphs remained visible; channels retained distinct line styles. Actual native Timeline mouse page movement, Page Down and Home worked without seeking frame 30. High contrast was then switched off by selecting None and Apply. This validates one real contrast scheme on this Windows machine, not every Windows theme/version or every popup state.

The automation launcher did not expose Settings when starting SystemSettings.exe directly; Explorer's address bar opened the documented Settings URIs instead. The computer-use foreground activation operation restores a maximized window, so activation-driven window restoration was not counted as an app theme-geometry failure. Occluded captures sometimes included the foreground Settings window; those were not counted as unobstructed Viewer visual evidence. Fresh foreground observations and stable accessibility text were used for final state checks.

## Native DPI integration results

- 125%: Comparator full integration passed 690 checks, process exit 0, actual window DPI 120, OS text factor 1.000 (`Tests/bin/x64/Release/comparer-native-125-report.txt`). Covers actual MFC two/three/four-source layouts, narrow/wide and unequal columns, pair-specific current/ROI/sequence results, scrollbar/keyboard/paint regressions, and both real ONNX LPIPS backends.
- 125% Viewer full integration passed 830 checks, exit 0 (`viewer-native-125-report.txt`); the separate startup run passed 39 checks, exit 0, actual startup DPI 120 and text factor 1.000 (`viewer-native-125-startup.txt`).
- 150%: Comparator full integration passed 690 checks, exit 0, actual DPI 144 (`comparer-native-150-report.txt`). Viewer full integration passed 830 checks and the startup run passed 39 checks, both exit 0; startup reports DPI 144 (`viewer-native-150-report.txt`, `viewer-native-150-startup.txt`).
- 175% before fix: the Comparator integration failed the existing small-window useful-image-canvas assertion after resizing to 800x500 (`comparer-native-175-report.txt`). This failure was not waived. All active settings were restored before editing/building.
- 175% after fix: Comparator full integration passed all 690 checks, exit 0, actual DPI 168 and text factor 1.000 (`comparer-native-175-after.txt`). The formerly failing narrow case now records a 292x118 image canvas, 44px source controls and 162px frame chrome; the required useful canvas height is 105px. The same sources/ROI/metrics/LPIPS and paint checks run to completion. A separate updated 100% run also passes (`comparer-layout-100-after.txt`).
- 175% Viewer full integration passed 830 checks, exit 0, and startup passed 39 checks, exit 0, actual startup DPI 168 (`viewer-native-175-report.txt`, `viewer-native-175-startup.txt`). OS text factor remains 1.000.
- 200%: absent from this display's regular scale list (100/125/150/175%). The list was inspected and scrolled. The custom scaling page permits entering 100-500%, warns of unreadable apps/text, affects all displays and warns that reverting can be difficult. Its input was left empty and Apply was not used. No custom scaling change, sign-out, restart or authentication was performed. Native 200% acceptance remains unverified; explicit 192-DPI layout/font inputs are tested separately, not relabelled as a live 200% test.

## Small-window high-DPI fix

The failure came from independently reserving preferred graph/current-result heights after a wrapped hosted menu consumed more vertical space. `AnalysisLayout.h` now partitions the actual remaining content height, preserves the roomy preferred layout, shrinks the scrollable graph then footer toward readable minima, and reserves a useful image viewport plus measured source-control height. Extremely small inputs stay nonnegative rather than producing invalid canvas sizes. MainFrm measures controls with the current DPI/text fonts instead of trusting cached old-DPI rectangles during a transition; ComparatorView clamps negative canvas heights. Source/metric/ROI state is not modified by this allocator.

The pure layout regression covers 5 DPI inputs (96/120/144/168/192), 5 text factors and 6 content heights, including extremely small inputs, available-space partitioning, readable panel minima when they fit, retained preferred heights when roomy, and useful source-space reservation. WindowsUiTypographyTests passes with warmed GDI count 46 before/46 after. A compile-time int/LONG mismatch in the canvas clamp was corrected before the successful build; no failed runtime assertion is bypassed.

CoreRegressionTests and ordinary Release/x64/v143 Comparator build pass. The new ordinary preview is `Comparator/x64/Issue139NativeReview/Comparator.exe`; it was inspected at 175% with all four demo videos, three graphs/counts (150/90/150), independent current results and real graph-click synchronized seeking to frame 32 (PSNR 33.7282/37.5699/29.1343 dB). The smaller scrollable panels do not clear analysis caches or replace source images. Updated-code full Comparator retests at 125% and 150% each pass 690 checks, exit 0, actual DPI 120/144 (`comparer-native-125-after.txt`, `comparer-native-150-after.txt`).

Ignored text reports are local test artifacts; this Markdown record is the durable summary. Relevant prior evidence: `issue-118-appearance.md` and `issue-139-reference-comparisons.md`.

Settings-page references: [Microsoft Settings URI reference](https://learn.microsoft.com/en-us/windows/apps/develop/launch/launch-settings), [Microsoft high-contrast handling guidance](https://learn.microsoft.com/en-us/windows/win32/winauto/high-contrast-parameter).

## Final handoff state

All active settings are restored: Windows/app colors Custom (Dark/Light, registry values 0/1), contrast None/off (Flags 126), display 100% at 1920x1080 landscape, Q1View shared Theme 2 (Dark). OS text size was never changed. Final 125% and 150% post-fix reports both exit 0 with 690 PASS checks and actual DPI 120/144; the 175% post-fix run exits 0 with 690 PASS checks at 168 DPI. The ordinary updated Comparator build passes and was reopened at the restored 100% scale with the four videos at frame 32 for review. Old temporary Comparator previews and Settings were closed; the Explorer folder was not changed. Git diff whitespace checks pass. No user-owned design files, remote issues/PRs, commits, merges, release tags, versions or Store state were changed.

The available native acceptance checks are complete on this machine, including fixing and repeating the discovered failure. Native 200% and mixed OS text-size/high-contrast combinations remain explicitly limited, not silently waived; this report does not declare #118/#139 closed or authorize their merge/release. Existing design/issue wording still needs reconciliation with the user's approved compact-under-source presentation before final issue/PR acceptance.

## Subsequent delivery authorization

After receiving the verification results and native 200% limitation, the user explicitly authorized the proposed separate #118/#139 PR delivery, merge and 2.8.0 release. That later approval is separate from the verification permission above. The theme PR carries shared painting/menu changes; the comparison PR carries the reference-pair model, compact per-candidate results, pair refresh queue and high-DPI analysis layout. Remote issue descriptions and release copy are reconciled with the approved presentation before delivery. Local development version remains `0.0.0-dev`; the tagged Actions build supplies `2.8.0`. The workflow performs final packaging/signing/release/Store work asynchronously; starting it is not reported as completed publication.
