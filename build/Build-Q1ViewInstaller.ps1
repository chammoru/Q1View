param(
    [string]$SourceDir = "",

    [string]$OutputDir = "",

    [string]$AppVersion = "0.0.0",

    [string]$OutputBaseFilename = "Q1ViewSetup-x64",

    [string]$InnoSetupCompiler = ""
)

$ErrorActionPreference = "Stop"

function Find-InnoSetupCompiler {
    $command = Get-Command "ISCC.exe" -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $candidates = @(
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
        "$env:ProgramFiles\Inno Setup 6\ISCC.exe"
    )

    foreach ($candidate in $candidates) {
        if ((-not [string]::IsNullOrWhiteSpace($candidate)) -and (Test-Path $candidate)) {
            return $candidate
        }
    }

    return ""
}

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
if ([string]::IsNullOrWhiteSpace($SourceDir)) {
    $SourceDir = Join-Path $repoRoot "dist\Q1View-windows-x64"
}
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $repoRoot "dist"
}
if ([string]::IsNullOrWhiteSpace($InnoSetupCompiler)) {
    $InnoSetupCompiler = Find-InnoSetupCompiler
}
if ([string]::IsNullOrWhiteSpace($InnoSetupCompiler)) {
    throw "ISCC.exe was not found. Install Inno Setup 6 or pass -InnoSetupCompiler."
}

$sourcePath = [System.IO.Path]::GetFullPath($SourceDir)
$outputPath = [System.IO.Path]::GetFullPath($OutputDir)
$scriptPath = Join-Path $repoRoot "installer\Q1View.iss"

foreach ($requiredFile in @("Viewer.exe", "Comparer.exe")) {
    $path = Join-Path $sourcePath $requiredFile
    if (-not (Test-Path $path)) {
        throw "Missing installer input: $path"
    }
}

New-Item -ItemType Directory -Force $outputPath | Out-Null

& $InnoSetupCompiler `
    "/DAppVersion=$AppVersion" `
    "/DSourceDir=$sourcePath" `
    "/DOutputDir=$outputPath" `
    "/DOutputBaseFilename=$OutputBaseFilename" `
    $scriptPath

if ($LASTEXITCODE -ne 0) {
    throw "Inno Setup compiler failed with exit code $LASTEXITCODE"
}

$installerPath = Join-Path $outputPath "$OutputBaseFilename.exe"
if (-not (Test-Path $installerPath)) {
    throw "Installer was not created: $installerPath"
}

Write-Host "Created installer: $installerPath"
