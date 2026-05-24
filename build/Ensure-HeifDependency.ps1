param(
    [Parameter(Mandatory = $true)]
    [string]$InstallRoot,

    [string]$ArchiveUrl = "",

    [string]$ArchivePath = "",

    [string]$Sha256 = "",

    [string]$Triplet = "x64-windows"
)

$ErrorActionPreference = "Stop"

function Test-HeifInstall {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Root
    )

    $heifHeader = Join-Path $Root "include\libheif\heif.h"
    $heifLib = Join-Path $Root "lib\heif.lib"
    return (Test-Path $heifHeader) -and (Test-Path $heifLib)
}

function Assert-Sha256 {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,

        [string]$ExpectedHash
    )

    if ([string]::IsNullOrWhiteSpace($ExpectedHash)) {
        Write-Warning "No SHA256 was provided for $FilePath. Set Q1VIEW_HEIF_SHA256 after publishing the dependency artifact."
        return
    }

    $actualHash = (Get-FileHash -LiteralPath $FilePath -Algorithm SHA256).Hash.ToLowerInvariant()
    $normalizedExpected = $ExpectedHash.Trim().ToLowerInvariant()
    if ($actualHash -ne $normalizedExpected) {
        throw "HEIF archive SHA256 mismatch. Expected $normalizedExpected, got $actualHash."
    }
}

function Get-ArchiveFileName {
    param([string]$Url)

    if ([string]::IsNullOrWhiteSpace($Url)) {
        return "Q1View-libheif-$Triplet.zip"
    }

    try {
        $uri = [System.Uri]$Url
        $name = [System.IO.Path]::GetFileName($uri.LocalPath)
        if (-not [string]::IsNullOrWhiteSpace($name)) {
            return $name
        }
    }
    catch {
    }

    return "Q1View-libheif-$Triplet.zip"
}

function Resolve-Archive {
    param(
        [string]$Url,
        [string]$Path,
        [string]$DownloadRoot,
        [string]$ExpectedHash
    )

    if (-not [string]::IsNullOrWhiteSpace($Path)) {
        $resolvedPath = [System.IO.Path]::GetFullPath($Path)
        if (-not (Test-Path $resolvedPath)) {
            throw "HEIF archive was not found at $resolvedPath"
        }

        Assert-Sha256 -FilePath $resolvedPath -ExpectedHash $ExpectedHash
        return $resolvedPath
    }

    if ([string]::IsNullOrWhiteSpace($Url)) {
        throw "HEIF archive URL is empty. Set Q1VIEW_HEIF_URL or Q1VIEW_HEIF_ARCHIVE."
    }

    New-Item -ItemType Directory -Force $DownloadRoot | Out-Null
    $downloadPath = Join-Path $DownloadRoot (Get-ArchiveFileName -Url $Url)

    if (Test-Path $downloadPath) {
        try {
            Assert-Sha256 -FilePath $downloadPath -ExpectedHash $ExpectedHash
            return $downloadPath
        }
        catch {
            Write-Warning $_.Exception.Message
            Write-Host "Re-downloading HEIF archive because the cached copy is invalid."
            Remove-Item -LiteralPath $downloadPath -Force
        }
    }

    Write-Host "Downloading HEIF dependency from $Url"
    Invoke-WebRequest -Uri $Url -OutFile $downloadPath
    Assert-Sha256 -FilePath $downloadPath -ExpectedHash $ExpectedHash
    return $downloadPath
}

function Clear-InstallRoot {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Root,

        [Parameter(Mandatory = $true)]
        [string]$RepoRoot
    )

    if (-not (Test-Path $Root)) {
        New-Item -ItemType Directory -Force $Root | Out-Null
        return
    }

    $depsRoot = [System.IO.Path]::GetFullPath((Join-Path $RepoRoot ".deps"))
    $fullRoot = [System.IO.Path]::GetFullPath($Root)
    $insideDeps = $fullRoot.StartsWith($depsRoot, [System.StringComparison]::OrdinalIgnoreCase)

    if ((-not $insideDeps) -and (-not (Test-HeifInstall -Root $Root))) {
        throw "Refusing to replace incomplete custom HEIF root: $fullRoot"
    }

    Remove-Item -LiteralPath $Root -Recurse -Force
    New-Item -ItemType Directory -Force $Root | Out-Null
}

function Copy-InstallPayload {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ExtractRoot,

        [Parameter(Mandatory = $true)]
        [string]$InstallRoot,

        [Parameter(Mandatory = $true)]
        [string]$Triplet
    )

    $payloadRoot = $ExtractRoot
    $nestedTripletRoot = Join-Path $ExtractRoot "installed\$Triplet"
    if (Test-HeifInstall -Root $nestedTripletRoot) {
        $payloadRoot = $nestedTripletRoot
    }

    if (-not (Test-HeifInstall -Root $payloadRoot)) {
        throw "The HEIF archive does not contain include\libheif\heif.h and lib\heif.lib at its root or under installed\$Triplet\."
    }

    Copy-Item -Path (Join-Path $payloadRoot "*") -Destination $InstallRoot -Recurse -Force
}

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$installRootPath = [System.IO.Path]::GetFullPath($InstallRoot)
$downloadRoot = Join-Path $repoRoot ".deps\downloads"

if (Test-HeifInstall -Root $installRootPath) {
    Write-Host "HEIF dependency is already installed at $installRootPath"
    exit 0
}

$mutex = New-Object System.Threading.Mutex($false, "Q1ViewEnsureHeifDependency")
$hasLock = $false

try {
    $hasLock = $mutex.WaitOne([TimeSpan]::FromMinutes(20))
    if (-not $hasLock) {
        throw "Timed out waiting for another process to finish preparing HEIF"
    }

    if (Test-HeifInstall -Root $installRootPath) {
        Write-Host "HEIF dependency is already installed at $installRootPath"
        exit 0
    }

    $archive = Resolve-Archive -Url $ArchiveUrl -Path $ArchivePath -DownloadRoot $downloadRoot -ExpectedHash $Sha256
    $extractRoot = Join-Path $downloadRoot "libheif-$Triplet-prebuilt-extract"

    if (Test-Path $extractRoot) {
        Remove-Item -LiteralPath $extractRoot -Recurse -Force
    }
    New-Item -ItemType Directory -Force $extractRoot | Out-Null

    Write-Host "Extracting HEIF dependency to $installRootPath"
    Expand-Archive -LiteralPath $archive -DestinationPath $extractRoot -Force

    Clear-InstallRoot -Root $installRootPath -RepoRoot $repoRoot
    Copy-InstallPayload -ExtractRoot $extractRoot -InstallRoot $installRootPath -Triplet $Triplet

    if (-not (Test-HeifInstall -Root $installRootPath)) {
        throw "HEIF dependency was extracted, but required files were not found under $installRootPath"
    }
}
finally {
    if ($hasLock) {
        $mutex.ReleaseMutex()
    }
    $mutex.Dispose()
}
