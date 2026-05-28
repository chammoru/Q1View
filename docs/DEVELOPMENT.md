# Q1View Development Guide

## Build Requirements

- Windows x64
- Visual Studio 2019 or newer with MSBuild and the C++ desktop workload
- PowerShell for dependency and packaging scripts
- Inno Setup 6 when creating the installer locally

## Build

From the repository root:

```powershell
msbuild Viewer\Viewer.sln /m /restore /p:Configuration=Release /p:Platform=x64
msbuild Comparator\Comparator.sln /m /restore /p:Configuration=Release /p:Platform=x64
```

Normal builds restore prebuilt OpenCV and libheif dependency archives into
`.deps`. The OpenCV archive is built with the VS2019 `v142` toolset so its
static libraries link with VS2019 and newer MSVC toolsets. Dedicated GitHub
Actions workflows create updated dependency archives when dependency versions
or toolsets change.

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
