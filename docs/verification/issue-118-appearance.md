# Issue #118: Windows appearance implementation and verification

Date: 2026-10-11. Branch: `codex/issue-118-appearance`, based on `origin/master` `cf0c29ebcefc30832e95711c3136b57a4d61c622`. Windows MFC Viewer and Comparator only. No Qt changes, new layout, font replacement, Mica dependency, version increase, release tag or Store submission.

## Behavior and boundaries

- Viewer: **View → Appearance**. Comparator: **Options → Appearance**. Choices are System (default), Light and Dark; each application's existing profile stores its own choice.
- The separate **Image background** submenu stores Neutral gray (`#303030`, default), Dark gray (`#181818`) or Light gray (`#ECECEC`). UI theme changes do not change this preference. Only exposed canvas pixels use that color; source, capture and metric inputs are unchanged.
- Shared role colors cover app-drawn titles, hosted menus and already owner-drawn popups, panels, help, text, selection, progress controls, status overlays and metric graphs. Folder labels use ordinary foreground text, not decorative blue. Essential progress/divider boundaries use a higher-contrast color than decorative separators.
- Windows high contrast overrides colors using system Window/WindowText/Highlight/HighlightText, without replacing saved preferences. Enabled supporting text uses WindowText; disabled menu text uses GrayText. Existing native-frame fallback remains. Graph channels also use different dash patterns so color is not their only distinction.
- Theme notification handlers repaint UI and update existing pens/brushes; they do not open files, rebuild folder entries, clear thumbnail caches or restart comparison scanning/playback. Thumbnail extension/loading badges are recolored in place; cached photo pixels and their indices remain intact. Thumbnail letterboxing is stable neutral gray, not baked from a mutable UI theme.
- Viewer scale-cache validity includes canvas color because its buffer contains exposed background pixels. A background change redraws that buffer, not the source decoder. UI-only theme changes keep the same canvas/cache key.
- Windows-owned dialogs, system menus, native/unstyled context menus, scrollbars and popup chrome remain OS-rendered. This job does not replace native popup tracking or introduce undocumented global dark-mode APIs.

## Local automated verification

Visual Studio 2022 Community MSBuild, Release/x64/v143. The existing generated local version remains `0.0.0-dev`; the version script was blocked by this shell's execution policy, and no policy was changed.

```powershell
$msbuild = 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe'
& $msbuild Tests\CoreRegressionTests.vcxproj /m /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143
.\Tests\bin\x64\Release\CoreRegressionTests.exe
& $msbuild Tests\WindowsUiTypographyTests.vcxproj /m /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143
.\Tests\bin\x64\Release\WindowsUiTypographyTests.exe
& $msbuild Viewer\Viewer.sln /m /restore /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 /p:IntDir=x64\Issue118Review\ /p:OutDir=x64\Issue118Review\
& $msbuild Comparator\Comparator.sln /m /restore /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 /p:IntDir=x64\Issue118Review\ /p:OutDir=x64\Issue118Review\
```

The appearance/typography executable passed pure System resolution and invalid-value fallback, preference persistence/reinitialization, Light/Dark × three independent canvas palettes, text contrast at least 4.5:1, essential control boundaries at least 3:1, semantic overlay contrast, high-contrast role mapping, BGR fills, native owner-drawn menu pixel readback and bounded brush/font GDI lifetime. It also retained the existing menu accessibility, caption/Snap hit-test, startup sizing and 4 DPI × 5 text-factor × 3 width tests. Final warmed GDI count: 126 before / 126 after.

Native integration suites use isolated test profiles, repository fixtures and disposable temporary copies, never private photographs. Build/run from the repository root:

```powershell
& $msbuild Viewer\Viewer.sln /m /restore /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 /p:Q1ViewGalleryTests=true /p:IntDir=x64\Issue118Gallery\ /p:OutDir=x64\Issue118Gallery\
$env:Q1VIEW_GALLERY_TEST_REPORT = Join-Path $PWD 'Tests/bin/x64/Release/issue-118-gallery-report.txt'
$test = Start-Process ./Viewer/x64/Issue118Gallery/Viewer.exe -ArgumentList '"Tests/fixtures/sample_16x16.png"' -WorkingDirectory $PWD -WindowStyle Hidden -PassThru -Wait
$test.ExitCode
& $msbuild Comparator\Comparator.sln /m /restore /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 /p:Q1ViewComparerTests=true /p:IntDir=x64\Issue118ComparerTests\ /p:OutDir=x64\Issue118ComparerTests\
$env:Q1VIEW_COMPARER_TEST_REPORT = Join-Path $PWD 'Tests/bin/x64/Release/issue-118-comparer-report.txt'
$test = Start-Process ./Comparator/x64/Issue118ComparerTests/Comparator.exe -WorkingDirectory $PWD -WindowStyle Hidden -PassThru -Wait
$test.ExitCode
```

Comparer exited 0 after actual menu-command theme/background transitions, preserving both source buffers, files/formats, zoom/pan, ROI, comparison/scan strategy and real PSNR/SSIM ROI scores. Four-source A/B/C/D checks read back presented canvas pixels and confirmed independent background colors and unchanged source buffers. Existing caption/menu movement and narrow-window checks also passed. Runtime DPI: 96; Windows text factor: 1.000.

The Viewer suite additionally checks theme/background transitions against the source buffer and decoder/open generation, source-space ROI and capture buffer, scaled exposed-canvas pixels, gallery generation/entries/selection/scroll and exact CPU/GPU thumbnail handles. During active video playback, UI-theme changes retain the existing timer, media and decoder generation. Existing gallery, drawer, GPU recreation, RAW, video, watcher and safe-recycle regression checks remain enabled.

Final expanded Viewer run exited 0 with `ALL INTEGRATION CHECKS PASSED`; final Comparer run exited 0 with `ALL COMPARER TYPOGRAPHY CHECKS PASSED` (the existing executable report name also covers its new appearance assertions). No failed assertion was waived. The initial normal build caught Windows `min`/`max` macro pollution introduced by the new header; defining `NOMINMAX` before its Windows include fixed that compile error, and both native app builds then succeeded.

GitHub gallery validation now watches the shared appearance/menu files and Comparator integration sources, runs the expanded appearance tests and builds/runs the Comparer suite. A workflow run is not used as a substitute for local evidence, and no release workflow was dispatched.

## Native review still pending

Computer-use initialization failed with `Computer Use native pipe is unavailable: failed to connect native pipe ... (os error 2)`. No alternate UI-input or screenshot mechanism was used. In-app regression pixel readbacks above are real rendering tests, but are not a visual review of native menu tracking, popup borders/chevrons or live Windows Settings changes.

- Review ordinary Viewer/Comparator Light/Dark menus, inactive captions, help, compact/gallery views, status messages and popup chrome on this machine.
- Confirm an actual Windows System-theme change while an image/video/comparison remains open.
- Confirm actual high-contrast on/off transitions, native-frame fallback, focus/selection and retained media/analysis/zoom/scroll state. Pure role-mapping tests do not prove live OS transitions or geometry preservation.
- Confirm live mixed-DPI/text-size combinations with themes if desired; only explicit DPI/text inputs and current 96-DPI native integration were exercised here.

Keep issue acceptance open and the PR in draft until native visual/high-contrast review is complete or its limits are explicitly accepted. No merge, release or automatic next-job progression is included.

System resolution uses Microsoft's documented [UISettings foreground-brightness approach](https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/ui/apply-windows-themes) and [ColorValuesChanged notification](https://learn.microsoft.com/uwp/api/windows.ui.viewmanagement.uisettings.colorvalueschanged). The caption uses documented [DWM attributes](https://learn.microsoft.com/windows/win32/api/dwmapi/ne-dwmapi-dwmwindowattribute); unsupported older-OS attributes can fail without blocking app colors.
