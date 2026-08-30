param(
    [string]$Configuration = "Release",

    [string]$Platform = "x64",

    [string]$OutputDir = "",

    [string]$ChangelogPath = "",

    [string[]]$ExcludedDllNames = @("libx265.dll"),

    [switch]$NoClean
)

$ErrorActionPreference = "Stop"

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $repoRoot "dist\Q1View-windows-x64"
}

$outputPath = [System.IO.Path]::GetFullPath($OutputDir)
$distRoot = [System.IO.Path]::GetFullPath((Join-Path $repoRoot "dist"))
if (-not $outputPath.StartsWith($distRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to clean or package outside the repository dist directory: $outputPath"
}

$viewerDir = Join-Path $repoRoot "Viewer\$Platform\$Configuration"
$comparerDir = Join-Path $repoRoot "Comparator\$Platform\$Configuration"
$viewerExe = Join-Path $viewerDir "Viewer.exe"
$comparerExe = Join-Path $comparerDir "Comparator.exe"

foreach ($requiredFile in @($viewerExe, $comparerExe)) {
    if (-not (Test-Path $requiredFile)) {
        throw "Missing build output: $requiredFile"
    }
}

if ((Test-Path $outputPath) -and -not $NoClean) {
    Remove-Item -LiteralPath $outputPath -Recurse -Force
}
New-Item -ItemType Directory -Force $outputPath | Out-Null

Copy-Item -LiteralPath $viewerExe -Destination $outputPath -Force
Copy-Item -LiteralPath $comparerExe -Destination $outputPath -Force

# Standalone file-association icons are consumed by the Inno installer. Keep
# them in the portable package too so every distribution contains the exact
# assets referenced by its registration metadata.
$iconOutputPath = Join-Path $outputPath "Icons"
New-Item -ItemType Directory -Force $iconOutputPath | Out-Null
foreach ($iconName in @("Q1ViewPhoto.ico", "Q1ViewVideo.ico", "Q1ViewRaw.ico")) {
    $iconSource = Join-Path $repoRoot "Viewer\res\$iconName"
    if (-not (Test-Path $iconSource)) {
        throw "Missing generated file-association icon: $iconSource`n  Run python build/render_icons.py."
    }
    Copy-Item -LiteralPath $iconSource -Destination $iconOutputPath -Force
}

$runtimeDlls = @()
foreach ($runtimeDir in @($viewerDir, $comparerDir)) {
    if (Test-Path $runtimeDir) {
        $runtimeDlls += Get-ChildItem -LiteralPath $runtimeDir -Filter "*.dll" -File
    }
}

$runtimeDlls |
    Sort-Object Name -Unique |
    Where-Object { $ExcludedDllNames -notcontains $_.Name } |
    ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $outputPath -Force
    }

foreach ($docFile in @("LICENSE", "README.md")) {
    $source = Join-Path $repoRoot $docFile
    if (Test-Path $source) {
        Copy-Item -LiteralPath $source -Destination $outputPath -Force
    }
}

if (-not [string]::IsNullOrWhiteSpace($ChangelogPath)) {
    $resolvedChangelog = [System.IO.Path]::GetFullPath($ChangelogPath)
    if (Test-Path $resolvedChangelog) {
        Copy-Item -LiteralPath $resolvedChangelog -Destination $outputPath -Force
    }
}

Write-Host "Packaged Q1View files into $outputPath"
Get-ChildItem -LiteralPath $outputPath -File | Sort-Object Name | ForEach-Object {
    Write-Host (" - {0}" -f $_.Name)
}
