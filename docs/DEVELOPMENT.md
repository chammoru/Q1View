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
msbuild Comparer\Comparer.sln /m /restore /p:Configuration=Release /p:Platform=x64
```

Normal builds restore prebuilt OpenCV and libheif dependency archives into
`.deps`. Dedicated GitHub Actions workflows create updated dependency archives
when dependency versions change.

## Package Locally

After both applications are built:

```powershell
.\build\Package-Q1View.ps1 -Configuration Release -Platform x64
.\build\Build-Q1ViewInstaller.ps1 -SourceDir .\dist\Q1View-windows-x64 -OutputDir .\dist
```

The portable directory contains `Viewer.exe`, `Comparer.exe`, and required
runtime DLLs together. The installer places the same runtime set on the target
machine and registers separate Viewer and Comparer Start Menu entries.

## Release Assets

Application releases publish two user-facing assets:

- `Q1ViewSetup-x64.exe`
- `Q1View-windows-x64.zip`

Dependency artifact releases are separate build inputs and are not end-user
application downloads.

## Documentation Assets

Product-facing documentation belongs in `docs/`, with screenshots in
`docs/images/`. Prefer current runtime captures over source-directory diagrams
or editable presentation files. When UI behavior changes, update the relevant
screen capture and user guide in the same change.
