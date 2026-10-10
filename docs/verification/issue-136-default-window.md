# Issue #136 — Windows Viewer default geometry

Date: 2026-10-11. Branch: `codex/issue-136-default-window`, based on `origin/master` at `996ac76`. Windows MFC Viewer only; no Qt behavior, RAW defaults, release version or tag changes.

## Implementation

The empty/default Viewer uses an 800×600-DIP baseline viewing area, separate from the existing `VIEWER_DEF_W/H` image/RAW defaults of 640×480. After initial document/menu creation, the frame computes the needed width with the same font, literal menu labels, mnemonic handling and padding as the responsive menu layout. The normal-size menu receives 24 DIP of additional clearance; the width grows beyond 800 DIP only if its actual measured labels require it. The outer window is bounded and positioned inside its monitor work area, including frame and title/menu chrome.

Startup geometry is applied only to `FileNew`, before showing the frame. The chosen width is settled before recomputing menu chrome and the final height, so an initially wrapped menu does not inflate the viewing area when it collapses to one row. File/Explorer/command-line opens retain the source-driven sizing path. DPI scales geometry, while Windows text enlargement does not force an oversized initial window: enlarged text and manual narrowing can still wrap the menu. The drawer continues consuming space within the same outer window and still starts closed. This does not introduce size persistence or minimum-width restrictions.

Menu measurement was extracted from `WindowsUiFrame::Layout` without changing its layout policy. Comparator shares that helper and was built and covered by the existing typography/menu tests; it does not receive the new startup geometry.

## Measurements and checks

Real GDI/Pretendard menu measurements, using detailed `640x480 (1.00x) · Auto` text:

| Explicit DPI | Menu without Update | Menu with Update | Chosen content width |
| --- | ---: | ---: | ---: |
| 96 / 100% | 587 px | 650 px | 800 px |
| 120 / 125% | 746 px | 826 px | 1000 px |
| 144 / 150% | 875 px | 968 px | 1200 px |

These are explicit font/DPI inputs, not claims that Windows Settings was changed. The actual MFC startup run reported DPI 96, OS text factor 1.000, content 800×600 and the empty menu's measured normal width 470 px. All eight initial buttons were on one row. Assertions also verified the outer rectangle fits its monitor, image/RAW dimensions remain 640×480, opening/closing the drawer never changes that rectangle, narrowing to 320 DIP wraps, and full-screen exit restores the original rectangle and empty image state.

`WindowsUiTypographyTests` passed, including the new policy/realistic-menu cases, small-work-area clamping, measurement-responsive width, existing wrapping/frame tests and font cache lifetime (GDI before/after 5/5). `CoreRegressionTests` passed. The full no-argument MFC gallery suite passed during implementation. On the final build, the separate no-argument startup run passed, including recomputing chrome after widening a wrapped menu; the command-line `sample_16x16.png` run passed its decoded-source-resolution assertion and the full gallery/GPU suite with `ALL INTEGRATION CHECKS PASSED`. Existing playback, selection, recycling and GPU recreation checks passed. Normal Viewer and Comparator Release/x64/v143 builds succeeded into the isolated `Issue136Review` output directory.

The gallery workflow now runs menu measurement/policy tests and a short no-argument startup test separately from its existing file-argument gallery/GPU run. Its path filters include the new geometry header, startup call site and shared menu measurement helper. `Q1VIEW_GALLERY_STARTUP_ONLY=1` selects only startup checks and explicitly identifies that mode in its report; it must not be reported as a full gallery/GPU run. Test executables use isolated preferences and repository/temp fixtures.

## Remaining native visual verification

The computer-use native connection was unavailable on two attempts (`failed to connect native pipe`, OS error 2). No Windows Settings, security policy or UI automation fallback was changed. Actual 125%/150% OS startup captures and visual review remain unverified; the explicit DPI table and actual 96-DPI MFC assertions are not substitutes. Keep the issue open until review/merge; no merge or release is authorized by this implementation request.

The optional local version-generation script was blocked by this shell's execution policy. The existing generated header was inspected and remains `0.0.0-dev` / `0.0.0.0`; no policy was changed and no release version was stamped into the checkout.

## Reproduction

Use VS2022 MSBuild with `Release`, `x64` and `PlatformToolset=v143`. Build `Tests/WindowsUiTypographyTests.vcxproj`, then run `Tests/bin/x64/Release/WindowsUiTypographyTests.exe`. Build `Viewer/Viewer.sln` with `/p:Q1ViewGalleryTests=true /p:IntDir=x64\UiTypographyIntegration\ /p:OutDir=x64\UiTypographyIntegration\`. Set `Q1VIEW_GALLERY_TEST_REPORT` to a writable report path and run `Viewer/x64/UiTypographyIntegration/Viewer.exe` without arguments for the empty startup path, or with `Tests/fixtures/sample_16x16.png` for the source-open path. Generated reports remain under ignored `Tests/bin/`.
