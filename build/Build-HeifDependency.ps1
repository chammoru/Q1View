param(
    [string]$VcpkgRoot = "",

    [string]$Triplet = "x64-windows",

    [string]$VcpkgRef = "aa40adda5352e87655b8583cfb2451d5e9e276fd",

    [string]$VcpkgPackage = "libheif",

    [string]$VcpkgFeatures = "aom"
)

$ErrorActionPreference = "Stop"

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,

        [string[]]$Arguments = @()
    )

    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$FilePath failed with exit code $LASTEXITCODE"
    }
}

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
if ([string]::IsNullOrWhiteSpace($VcpkgRoot)) {
    $VcpkgRoot = Join-Path $repoRoot ".deps\vcpkg"
}

$vcpkgRootPath = [System.IO.Path]::GetFullPath($VcpkgRoot)
$heifRoot = Join-Path $vcpkgRootPath "installed\$Triplet"
$heifHeader = Join-Path $heifRoot "include\libheif\heif.h"
$heifLib = Join-Path $heifRoot "lib\heif.lib"

if (-not (Test-Path $vcpkgRootPath)) {
    $parent = Split-Path -Parent $vcpkgRootPath
    if ($parent -and -not (Test-Path $parent)) {
        New-Item -ItemType Directory -Force $parent | Out-Null
    }

    Write-Host "Cloning vcpkg into $vcpkgRootPath"
    Invoke-Checked -FilePath "git" -Arguments @("clone", "https://github.com/microsoft/vcpkg.git", $vcpkgRootPath)
}

if (-not [string]::IsNullOrWhiteSpace($VcpkgRef)) {
    Write-Host "Checking out vcpkg ref $VcpkgRef"
    Invoke-Checked -FilePath "git" -Arguments @("-C", $vcpkgRootPath, "fetch", "--depth", "1", "origin", $VcpkgRef)
    Invoke-Checked -FilePath "git" -Arguments @("-C", $vcpkgRootPath, "checkout", "--force", "FETCH_HEAD")
}

$vcpkgExe = Join-Path $vcpkgRootPath "vcpkg.exe"
if (-not (Test-Path $vcpkgExe)) {
    $bootstrap = Join-Path $vcpkgRootPath "bootstrap-vcpkg.bat"
    if (-not (Test-Path $bootstrap)) {
        throw "vcpkg bootstrap script was not found: $bootstrap"
    }

    Write-Host "Bootstrapping vcpkg"
    Invoke-Checked -FilePath $bootstrap -Arguments @("-disableMetrics")
}

$manifestRoot = Join-Path $repoRoot ".deps\heif-vcpkg-manifest"
New-Item -ItemType Directory -Force $manifestRoot | Out-Null

$features = @(
    $VcpkgFeatures.Split(",", [System.StringSplitOptions]::RemoveEmptyEntries) |
        ForEach-Object { $_.Trim() } |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
)

$dependency = [ordered]@{
    name = $VcpkgPackage
    "default-features" = $false
}
if ($features.Count -gt 0) {
    $dependency.features = $features
}

$manifest = [ordered]@{
    name = "q1view-heif-dependency"
    "version-string" = "0"
    dependencies = @($dependency)
}
$manifestPath = Join-Path $manifestRoot "vcpkg.json"
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding ASCII

Write-Host "Installing $VcpkgPackage with default features disabled and features: $($features -join ', ')"
Push-Location $manifestRoot
try {
    Invoke-Checked -FilePath $vcpkgExe -Arguments @(
        "install",
        "--triplet", $Triplet,
        "--x-install-root", (Join-Path $vcpkgRootPath "installed"),
        "--recurse"
    )
}
finally {
    Pop-Location
}

if (-not ((Test-Path $heifHeader) -and (Test-Path $heifLib))) {
    throw "libheif install completed, but required files were not found under $heifRoot"
}
