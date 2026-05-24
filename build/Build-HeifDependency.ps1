param(
    [string]$VcpkgRoot = "",

    [string]$Triplet = "x64-windows",

    [string]$VcpkgRef = "aa40adda5352e87655b8583cfb2451d5e9e276fd",

    [string]$VcpkgPackage = "libheif[aom]"
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

Write-Host "Installing ${VcpkgPackage}:$Triplet"
Invoke-Checked -FilePath $vcpkgExe -Arguments @("install", "${VcpkgPackage}:$Triplet")

if (-not ((Test-Path $heifHeader) -and (Test-Path $heifLib))) {
    throw "libheif install completed, but required files were not found under $heifRoot"
}
