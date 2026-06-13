# Repository Guidance

## Build Environment

This repository builds on Windows x64 with Visual Studio 2022, the MSVC v143
toolset, and MFC.

MSBuild may be installed even when it is not available on `PATH`. Do not
conclude that local Windows builds are unavailable just because
`where.exe msbuild` or `Get-Command msbuild` fails.

When `msbuild` is not on `PATH`, locate it with Visual Studio's `vswhere.exe`:

```powershell
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" |
    Select-Object -First 1
& $msbuild Tests\CoreRegressionTests.vcxproj /m /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143
```

On this machine, MSBuild has previously been found at:

```text
C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe
```

## Verification

For release-sensitive changes, prefer this local verification path before
falling back to GitHub Actions:

```powershell
.\build\Write-Q1ViewVersion.ps1
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" |
    Select-Object -First 1
& $msbuild Tests\CoreRegressionTests.vcxproj /m /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143
.\Tests\bin\x64\Release\CoreRegressionTests.exe
& $msbuild Viewer\Viewer.sln /m /restore /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143
& $msbuild Comparator\Comparator.sln /m /restore /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143
```

Use the GitHub Actions release workflow as the final authority for tagged
release builds, packaging, signing, GitHub Release creation, and Microsoft
Store submission.

## Changelog and release notes

**Write each changelog bullet and paragraph as a single logical line. Do not
hard-wrap (manually break) lines inside a bullet or paragraph.** This applies to
`CHANGELOG.md` and to the copy blocks in `docs/STORE_LISTING.md`.

Why this matters — the release pipeline reuses the changelog two different ways:

- **GitHub Release notes** (`RELEASE_NOTES.md`) are rendered as Markdown, so
  soft-wrapped lines are joined back into one paragraph. Hard wraps look fine
  here, which is exactly why this trap stays invisible until you check the Store.
- **Microsoft Store "What's New"** (`STORE_WHATS_NEW.txt`) is built from the same
  matching `CHANGELOG.md` section but emitted as **plain text** and sent to
  Partner Center verbatim (see the "Prepare release notes" and "Publish MSIX"
  steps in `.github/workflows/release.yml`). The Store renders every newline
  literally and then wraps the text again, so a pre-wrapped bullet shows ragged,
  mid-phrase line breaks. This is the same defect that issue #71 fixed in the
  Store description.

So: one line per bullet, however long. Let Markdown and the Store do their own
wrapping. Keep blank lines between bullets, the `## [x.y.z] — date` headers, the
`---` rules, and the link-reference block at the bottom as they are.

```text
Wrong (hard-wrapped — breaks the Store "What's New"):
- Viewer: press `D` to cycle through the preset resolutions for raw input,
  mirroring how `N` advances the color space.

Right (one logical line):
- Viewer: press `D` to cycle through the preset resolutions for raw input, mirroring how `N` advances the color space.
```

### Cutting a release

1. Move the `[Unreleased]` items into a new `## [x.y.z] — YYYY-MM-DD` section,
   keeping the one-line-per-bullet rule above. Add the matching
   `[x.y.z]: …/compare/…` link at the bottom and update the `[Unreleased]`
   compare link.
2. If a feature is now user-visible, keep `docs/STORE_LISTING.md` (description,
   feature list, search terms) consistent with it — same no-hard-wrap rule.
3. Verify locally (see Verification above), then push the `vx.y.z` tag. The
   release workflow builds the GitHub Release notes and the Store "What's New"
   from the section you just wrote; there is no separate place to edit them.
