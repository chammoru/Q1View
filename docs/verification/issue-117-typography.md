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
