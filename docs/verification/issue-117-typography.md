# Issue #117: Windows UI typography verification

Date: 2026-10-04. Branch: `codex/issue-117-typography`, based on `origin/master` `7b8d50f57eb0738d85d73aeba86c05f94120a4a9`. Windows MFC applications only; no Qt, image-processing, metric-input, theme, native menu owner-draw, release-tag or version changes.

## Implementation

- Retain the bundled Pretendard family and Segoe UI fallback. Use shared negative character-height GDI role definitions for body, commands, captions, folders, supporting text and status. Aligned numeric values use installed Cascadia Mono or Consolas.
- Replace Viewer/Comparator help's all-Consolas, space-aligned paragraphs with measured columns, wrapped descriptions and bounded scrolling. Preserve commands; wheel/arrows/Page Up/Page Down/Home/End navigate help only while it is open, and Esc closes it.
- Reuse role fonts; invalidate GDI/DirectWrite resources on Windows text-size notification, marshal notifications onto the UI thread and remeasure affected control bounds. Preserve CPU thumbnail cache when recreating GPU text resources.
- Scale timeline/graph/header and video progress text areas. Restore selected fonts and persistent render DC state before font replacement or deletion. Keep image-space pixel-label sizes zoom-derived, not accessibility-scaled.

## Local automated checks

Use Visual Studio 2022 MSBuild, Release/x64/v143. `build/Write-Q1ViewVersion.ps1` generates the local development version; do not replace it with a release number.

```powershell
$msbuild = 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe'
& $msbuild Tests\CoreRegressionTests.vcxproj /m /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143
.\Tests\bin\x64\Release\CoreRegressionTests.exe
& $msbuild Tests\WindowsUiTypographyTests.vcxproj /m /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143
.\Tests\bin\x64\Release\WindowsUiTypographyTests.exe
& $msbuild Viewer\Viewer.sln /m /restore /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143
& $msbuild Comparator\Comparator.sln /m /restore /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143
```

Results: core regression tests passed; both native Release applications built successfully. Typography tests passed for explicit DPI values 96/120/144/192, text factors 1/1.25/1.5/2/2.25 and help widths 320/640/960 DIP. Tests check actual bundled font resolution, Korean/English glyph availability, font heights and reuse, independent wrapped columns, access to the final row, navigation routing, persistent DC font restoration and bounded cache lifetime. GDI object counts after warm-up: before 2, after 2.

Build the existing native gallery integration suite in an isolated output directory, then run from the repository root so its fixture lookup succeeds:

```powershell
& $msbuild Viewer\Viewer.sln /m /restore /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 /p:Q1ViewGalleryTests=true /p:IntDir=x64\UiTypographyIntegration\ /p:OutDir=x64\UiTypographyIntegration\
$env:Q1VIEW_GALLERY_TEST_REPORT = Join-Path $PWD 'Tests/bin/x64/Release/typography-gallery-report.txt'
$test = Start-Process -FilePath (Join-Path $PWD 'Viewer/x64/UiTypographyIntegration/Viewer.exe') -WorkingDirectory $PWD -WindowStyle Hidden -PassThru -Wait
$test.ExitCode
Remove-Item Env:Q1VIEW_GALLERY_TEST_REPORT
```

The suite uses an isolated test registry profile and repository fixtures/temporary copies, not private photographs. Coverage includes bundled native GDI/DirectWrite font use, long wrapped folder names, multi-selection, E drawer transitions and key repeat, current-image/zoom/pan preservation, directory changes, playback, bounded thumbnail memory and actual GPU-resource recreation. The final run after the font/DC-lifetime and progress-band fixes exited 0 with `ALL INTEGRATION CHECKS PASSED`. A first invocation from the executable directory failed fixture lookup; running from the repository root corrected that test-environment error.

## Native visual checks and remaining limits

Viewer and Comparator were launched and inspected on this machine. Empty-state prose, Comparator headers and help columns were legible. Viewer help could be scrolled to its final row with End; Comparator's `Drag & Drop` retained its literal ampersand, and Esc closed help.

Explicit DPI/text-scale inputs in the automated tests are not evidence of live Windows Settings changes or cross-monitor DPI behavior. Real monitor moves at 100/125/150/200%, live OS text enlargement, every missing-script/emoji fallback case, and a full Comparator ROI/metric interaction matrix still require native visual acceptance. Keep the issue's full acceptance boxes open until those checks are performed; this implementation does not authorize the next design job or a release.

Windows text scaling follows [Microsoft's text-scaling guidance](https://learn.microsoft.com/en-us/windows/apps/develop/input/text-scaling) using UISettings2, without changing system settings.

## Approved menu extension

After inspecting the native menu/body balance, the user explicitly requested code changes to the menus. Main application menu bars and their dropdown trees now use normal-weight Pretendard 14 DIP with wider horizontal text padding. Dropdowns request at least 30 DIP row height; Windows still owns the native menu-bar height, and the bar font fits within that native hit-tested row rather than changing global system settings. This is not a new toolbar or a replacement menu-tracking implementation. System menus, unrelated context menus and dialogs are not restyled.

Real HMENUs and command IDs, enabled/disabled states, check/radio states, submenu trees and native keyboard tracking remain. Owner-drawn items expose native names using [Microsoft's MSAAMENUINFO mechanism](https://learn.microsoft.com/en-us/windows/win32/api/oleacc/ns-oleacc-msaamenuinfo). WM_MENUCHAR handles ampersand mnemonics, including disabled items and duplicate cycling; dynamic ModifyMenu/InsertMenu labels are synchronized without aliasing item metadata. Source-string parsers query the retained text, not GetMenuString's owner-draw limitations.

The typography test executable additionally verifies menu metadata, accessible names, retained states, mnemonic routing, dynamic source strings, insertion, 30 DIP requested measurement and font restoration. Native menu-bar height must be assessed in the real window: a WM_MEASUREITEM request alone is not proof that Windows raised it to 30 DIP. Live enlarged-text and mixed-monitor acceptance remains open.

Native review found and corrected two display defects in the prototype: querying menu-item rectangles while drawing could re-enter menu layout and hide startup labels, so the bar now uses system DPI-aware menu metrics; owner-draw painting must explicitly render check/radio marks and submenu arrows, so these now use the retained native states. The test renders a checked radio into a DIB and verifies its pixels. The final typography test's warmed GDI count is 3 before / 3 after (the earlier body-only run was 2/2).

The integration rerun initially reached the deferred-folder-activation assertion before its posted message had been processed. That assertion used a fixed 50 ms pump; it now uses the suite's existing bounded Await helper to check the actual asynchronous outcome. No product folder-navigation code was changed, and the subsequent full integration run passed. Keep this timing repair distinct from menu appearance verification.
