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
