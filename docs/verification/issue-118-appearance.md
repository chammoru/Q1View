# Issue #118: Windows appearance implementation and verification

Date: 2026-10-11. Branch: `codex/issue-118-appearance`, based on `origin/master` `cf0c29ebcefc30832e95711c3136b57a4d61c622`. Windows MFC Viewer and Comparator only. Following native user review, the user explicitly approved replacing the initial independent appearance/background design with one Theme control and matching Options placement in both apps. No Qt changes, layout redesign, font replacement, Mica dependency, version increase, release tag or Store submission.

## Behavior and boundaries

- Both applications: **Options → Theme → System / Light / Dark**. Viewer retains **View → Image Scaling**; Comparator retains its existing Allow Different Resolutions option. There is no separate Appearance or Image background submenu.
- Light uses bright UI colors with a light gray surround (`#ECECEC`); Dark uses dark UI colors with a dark gray surround (`#181818`). System resolves both together from the Windows app theme. Source, capture and metric input pixels are unchanged.
- The existing `Appearance/Theme` profile key remains compatible. Obsolete `Appearance/Canvas` values are ignored, not deleted, so an old separate background cannot override the new theme after restart. Each app continues to remember its own theme.
- Shared role colors cover app-drawn titles, hosted menus and already owner-drawn popups, panels, help, text, selection, progress controls, status overlays and metric graphs. Folder labels use ordinary foreground text, not decorative blue. Essential progress/divider boundaries use a higher-contrast color than decorative separators.
- Windows high contrast overrides colors using system Window/WindowText/Highlight/HighlightText, without replacing saved preferences. Enabled supporting text uses WindowText; disabled menu text uses GrayText. Existing native-frame fallback remains. Graph channels also use different dash patterns so color is not their only distinction.
- Theme notification handlers repaint UI and update existing pens/brushes; they do not open files, rebuild folder entries, clear thumbnail caches or restart comparison scanning/playback. Thumbnail extension/loading badges are recolored in place; cached photo pixels and their indices remain intact. Thumbnail letterboxing is stable neutral gray, not baked from a mutable UI theme.
- Viewer scale-cache validity includes canvas color because its buffer contains exposed background pixels. A theme change redraws the buffer with the matching surround without restarting the source decoder.
- Windows-owned dialogs, system menus, native/unstyled context menus, scrollbars and popup chrome remain OS-rendered. This job does not replace native popup tracking or introduce undocumented global dark-mode APIs.

## Local automated verification

Visual Studio 2022 Community MSBuild, Release/x64/v143. The existing generated local version remains `0.0.0-dev`; the version script was blocked by this shell's execution policy, and no policy was changed.

```powershell
$msbuild = 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe'
& $msbuild Tests\CoreRegressionTests.vcxproj /m /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143
.\Tests\bin\x64\Release\CoreRegressionTests.exe
& $msbuild Tests\WindowsUiTypographyTests.vcxproj /m /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143
.\Tests\bin\x64\Release\WindowsUiTypographyTests.exe
& $msbuild Viewer\Viewer.sln /m /restore /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 /p:IntDir=x64\Issue118ThemeReview\ /p:OutDir=x64\Issue118ThemeReview\
& $msbuild Comparator\Comparator.sln /m /restore /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 /p:IntDir=x64\Issue118ThemeReview\ /p:OutDir=x64\Issue118ThemeReview\
```

The appearance/typography executable passed pure System/Light/Dark resolution with matching surround colors, invalid-value fallback, theme persistence/reinitialization with ignored legacy Canvas values, retired-command rejection, a single three-choice Theme submenu with no duplicate controls or leading separator, text contrast at least 4.5:1, essential control boundaries at least 3:1, semantic overlay contrast, high-contrast role mapping, BGR fills, native owner-drawn menu pixel readback and bounded brush/font GDI lifetime. It also retained the existing menu accessibility, caption/Snap hit-test, startup sizing and 4 DPI × 5 text-factor × 3 width tests. Final warmed GDI count: 46 before / 46 after. The realistic Viewer menu including Options measured 648 px at 96 DPI, or 711 px with Update, within the default 800 px content width.

Native integration suites use isolated test profiles, repository fixtures and disposable temporary copies, never private photographs. Build/run from the repository root:

```powershell
& $msbuild Viewer\Viewer.sln /m /restore /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 /p:Q1ViewGalleryTests=true /p:IntDir=x64\Issue118Gallery\ /p:OutDir=x64\Issue118Gallery\
$env:Q1VIEW_GALLERY_TEST_REPORT = Join-Path $PWD 'Tests/bin/x64/Release/issue-118-theme-gallery-report.txt'
$test = Start-Process ./Viewer/x64/Issue118Gallery/Viewer.exe -ArgumentList '"Tests/fixtures/sample_16x16.png"' -WorkingDirectory $PWD -WindowStyle Hidden -PassThru -Wait
$test.ExitCode
& $msbuild Comparator\Comparator.sln /m /restore /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 /p:Q1ViewComparerTests=true /p:IntDir=x64\Issue118ComparerTests\ /p:OutDir=x64\Issue118ComparerTests\
$env:Q1VIEW_COMPARER_TEST_REPORT = Join-Path $PWD 'Tests/bin/x64/Release/issue-118-theme-comparer-report.txt'
$test = Start-Process ./Comparator/x64/Issue118ComparerTests/Comparator.exe -WorkingDirectory $PWD -WindowStyle Hidden -PassThru -Wait
$test.ExitCode
```

Comparer integration checks exercise actual menu-command theme transitions, the same Options > Theme location, both source buffers, files/formats, zoom/pan, ROI, comparison/scan strategy and real PSNR/SSIM ROI scores. Four-source A/B/C/D checks read back presented canvas pixels to confirm matching theme backgrounds and unchanged source buffers. Existing caption/menu movement and narrow-window checks remain enabled. Runtime DPI in the updated passing run: 96; Windows text factor: 1.000.

The Viewer suite additionally checks theme transitions against the source buffer and decoder/open generation, source-space ROI and capture buffer, matching scaled exposed-canvas pixels, gallery generation/entries/selection/scroll and exact CPU/GPU thumbnail handles. It asserts that only Options > Theme exposes theme controls and View retains Image Scaling without duplicate appearance/background entries. During active video playback, theme changes retain the existing timer, media and decoder generation. Existing gallery, drawer, GPU recreation, RAW, video, watcher and safe-recycle regression checks remain enabled.

After the approved Theme simplification, CoreRegressionTests, GalleryLayoutTests, WindowsUiTypographyTests, the full Viewer integration suite, default Viewer/RAW startup and Comparer integration all exited 0. Reports are `Tests/bin/x64/Release/issue-118-theme-gallery-report.txt`, `issue-118-theme-startup-report.txt` and `issue-118-theme-comparer-report.txt` (ignored local artifacts). No failed assertion is waived. The initial implementation's normal build caught Windows `min`/`max` macro pollution introduced by the new header; defining `NOMINMAX` before its Windows include fixed that compile error.

Ordinary Viewer and Comparator use a separate `Issue118ThemeReview` intermediate/output directory, without integration-test switches. Reusing the previous intermediate directory initially produced an LNK4020 stale PDB warning; compiling into fresh matching intermediate/output directories removed that artifact warning. These ordinary binaries, not the test-instrumented applications, are the review builds.

GitHub gallery validation now watches the shared appearance/menu files and Comparator integration sources, runs the expanded appearance tests and builds/runs the Comparer suite. A workflow run is not used as a substitute for local evidence, and no release workflow was dispatched.

## Native review still pending

Computer-use initialization failed with `Computer Use native pipe is unavailable: failed to connect native pipe ... (os error 2)`. No alternate UI-input or screenshot mechanism was used. In-app regression pixel readbacks above are real rendering tests, but are not a visual review of native menu tracking, popup borders/chevrons or live Windows Settings changes.

- Review ordinary Viewer/Comparator Light/Dark menus, inactive captions, help, compact/gallery views, status messages and popup chrome on this machine.
- Confirm an actual Windows System-theme change while an image/video/comparison remains open.
- Confirm actual high-contrast on/off transitions, native-frame fallback, focus/selection and retained media/analysis/zoom/scroll state. Pure role-mapping tests do not prove live OS transitions or geometry preservation.
- Confirm live mixed-DPI/text-size combinations with themes if desired; only explicit DPI/text inputs and current 96-DPI native integration were exercised here.

Keep issue acceptance open and the PR in draft until native visual/high-contrast review is complete or its limits are explicitly accepted. No merge, release or automatic next-job progression is included.

System resolution uses Microsoft's documented [UISettings foreground-brightness approach](https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/ui/apply-windows-themes) and [ColorValuesChanged notification](https://learn.microsoft.com/uwp/api/windows.ui.viewmanagement.uisettings.colorvalueschanged). The caption uses documented [DWM attributes](https://learn.microsoft.com/windows/win32/api/dwmapi/ne-dwmapi-dwmwindowattribute); unsupported older-OS attributes can fail without blocking app colors.
