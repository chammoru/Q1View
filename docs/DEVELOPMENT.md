# Q1View Development Guide

## Build Requirements

- Windows x64
- Visual Studio 2022 with MSBuild, the C++ desktop workload, the MSVC v143
  toolset, and the Visual C++ MFC component
- PowerShell for dependency and packaging scripts
- Inno Setup 6 when creating the installer locally

## Build

From the repository root:

```powershell
.\build\Write-Q1ViewVersion.ps1
msbuild Tests\CoreRegressionTests.vcxproj /m /p:Configuration=Release /p:Platform=x64
.\Tests\bin\x64\Release\CoreRegressionTests.exe
msbuild Viewer\Viewer.sln /m /restore /p:Configuration=Release /p:Platform=x64
msbuild Comparator\Comparator.sln /m /restore /p:Configuration=Release /p:Platform=x64
```

To stage the portable package in `dist\Q1View-windows-x64`, run:

```powershell
.\build\Package-Q1View.ps1 -Configuration Release -Platform x64
```

Install Visual Studio 2022 first if it is not already present. If Visual Studio
2022 is installed but the C++ workload or MFC component is missing, add them
once from an elevated PowerShell:

```powershell
& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\setup.exe" modify --installPath "C:\Program Files\Microsoft Visual Studio\2022\Community" --add Microsoft.VisualStudio.Workload.NativeDesktop --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --add Microsoft.VisualStudio.Component.VC.ATLMFC --passive --norestart
```

## Qt Viewer (Experimental, Cross-Platform)

`ViewerQt/` is an experimental Qt 6 port of the viewer that also builds on macOS
and Linux. It is configured through the top-level `CMakeLists.txt`, separately
from the MFC Viewer/Comparator MSBuild projects above.

### Prerequisites

- Qt 6.5.3 (on Windows, the `msvc2019_64` kit) **including the `qtmultimedia`
  module**. Video playback (`ViewerQt/VideoView.cpp`) links `Qt6::Multimedia` /
  `Qt6::MultimediaWidgets`; when that module is absent, CMake prints
  `video playback disabled (Qt6::Multimedia not found)` and silently drops the
  video page while the rest of the viewer still builds. Qt's official binaries
  ship a self-contained FFmpeg media backend, so the player handles the common
  container/codec set — MP4/H.264, MKV, WebM, AVI, WMV, … — with no extra
  runtime DLLs.
- libheif for HEIF/HEIC/AVIF image decode (optional). On Windows this is the same
  prebuilt "decode-av1" root the MFC product restores into
  `.deps/libheif-decode-av1-x64-windows` (run `build/Ensure-HeifDependency.ps1`,
  as the `viewer-qt` workflow does).

If an existing Qt install is missing `qtmultimedia` — the symptom is video files
not playing at all — add it without reinstalling Qt, via the Qt Maintenance Tool
or `aqtinstall` (module archive only, no base re-download):

```powershell
pip install aqtinstall
python -m aqt install-qt windows desktop 6.5.3 win64_msvc2019_64 `
    -m qtmultimedia --archives qtmultimedia --outputdir D:\Qt
```

### Build

```powershell
cmake -S . -B build-qt -DQ1VIEW_BUILD_QT_VIEWER=ON -DQ1VIEW_BUILD_SMOKE_TEST=OFF `
    -DCMAKE_PREFIX_PATH=D:\Qt\6.5.3\msvc2019_64
cmake --build build-qt --config Release --target q1view_viewer_qt --parallel
```

`.github/workflows/viewer-qt.yml` builds the same target on Windows, macOS, and
Linux and is the authoritative reference for packaging (`windeployqt` /
`macdeployqt` / `linuxdeploy` bundle the Qt multimedia plugins next to the
executable).

## Application Version

Viewer and Comparator compile their Windows file version resources from
`QCommon/inc/Q1ViewVersion.h`. At runtime, the visible Help/About version is
read back from the current executable's `VS_VERSION_INFO` `ProductVersion`
value with the Windows version APIs. Local builds use `0.0.0-dev` by default.
Release CI derives `X.Y.Z` from the release tag `vX.Y.Z`, regenerates
`QCommon/inc/Q1ViewVersion.h` before compiling, and passes `X.Y.Z.0` to the
MSIX packager.

To test a release-version build locally:

```powershell
.\build\Write-Q1ViewVersion.ps1 -Version 2.2.0
msbuild Viewer\Viewer.sln /m /restore /p:Configuration=Release /p:Platform=x64
msbuild Comparator\Comparator.sln /m /restore /p:Configuration=Release /p:Platform=x64
```

Normal builds restore prebuilt OpenCV and libheif dependency archives into
`.deps`. The project files target MSVC v143. The bundled OpenCV archive name
still includes `v142` because that is the prebuilt dependency package currently
published for Q1View; it remains ABI-compatible with the VS2022 toolset used by
the app projects. Dedicated GitHub Actions workflows create updated dependency
archives when dependency versions or toolsets change.

## Core Regression Tests

The portable core suite verifies raw layout sizing, source-native pixel
sampling, RGB conversion outputs, and PSNR/SSIM reference values with small
deterministic fixtures.

On macOS or Linux:

```sh
./Tests/run_core_regression_tests.sh
```

From a Visual Studio Developer PowerShell on Windows:

```powershell
msbuild Tests\CoreRegressionTests.vcxproj /m /p:Configuration=Release /p:Platform=x64
.\Tests\bin\x64\Release\CoreRegressionTests.exe
```

The Windows GitHub Actions build runs this suite before building the
applications.

## Package Locally

After both applications are built:

```powershell
.\build\Package-Q1View.ps1 -Configuration Release -Platform x64
.\build\Build-Q1ViewInstaller.ps1 -SourceDir .\dist\Q1View-windows-x64 -OutputDir .\dist
```

The portable directory contains `Viewer.exe`, `Comparator.exe`, and required
runtime DLLs together. The installer places the same runtime set on the target
machine and registers separate Viewer and Comparator Start Menu entries.

## Release Assets

Application releases publish two user-facing assets:

- `Q1ViewSetup-x64.exe`
- `Q1View-windows-x64.zip`

Dependency artifact releases are separate build inputs and are not end-user
application downloads.

## MSIX Packaging

MSIX is required for Microsoft Store submission and supports side-loading on
Windows 10 (version 1809 or later) and Windows 11.

### Prerequisites

- Windows 10 SDK — provides `makeappx.exe` and `signtool.exe`.
  Install via Visual Studio Installer → Individual components → Windows 10 SDK.
- A code-signing certificate whose Subject CN matches the `Publisher` value in
  `installer/msix/AppxManifest.xml`.
  - **Microsoft Store**: use the certificate assigned in Partner Center.
  - **Side-loading / development**: a self-signed certificate works.
- Logo PNG assets in `installer/msix/Assets/` (see
  `installer/msix/Assets/README.md` for required sizes).

### Build an MSIX package locally

Build the applications first, then run:

```powershell
# Stage binaries
.\build\Package-Q1View.ps1 -Configuration Release -Platform x64

# Create and sign the MSIX
.\build\Package-Q1ViewMsix.ps1 `
    -AppVersion 1.0.16.0 `
    -Publisher "CN=YourPublisherCN"
```

The signed package is written to `dist\Q1View-windows-x64.msix`.

To validate the manifest without signing:

```powershell
.\build\Package-Q1ViewMsix.ps1 -AppVersion 1.0.0.0 -SkipSigning
```

### Manifest

`installer/msix/AppxManifest.xml` defines the package identity, the Viewer
application entry point, and the `runFullTrust` capability required for
packaged Win32 applications. `Comparator.exe` remains in the MSIX payload and
is launched through Viewer's **Compare** action; declaring it as a second
Store entry point fails WACK package compliance.

Before Store submission, update `Identity/@Publisher` to match the exact
Publisher string shown in Microsoft Partner Center. The `Version` and
`Publisher` fields are overridden at build time by `Package-Q1ViewMsix.ps1`.

### Sideloading

To install the MSIX outside the Store:

1. Install the signing certificate as a Trusted Root CA (one-time setup).
2. Double-click `Q1View-windows-x64.msix` or run:
   ```powershell
   Add-AppPackage -Path .\dist\Q1View-windows-x64.msix
   ```

## Documentation Assets

Product-facing documentation belongs in `docs/`, with screenshots in
`docs/images/`. Prefer current runtime captures over source-directory diagrams
or editable presentation files. When UI behavior changes, update the relevant
screen capture and user guide in the same change.
