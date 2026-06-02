param(
    [string]$Version = "1.26.0",

    [Parameter(Mandatory = $true)]
    [string]$InstallRoot,

    [string]$ArchiveUrl = "",

    [string]$ArchivePath = "",

    [string]$Sha256 = "",

    [string]$Platform = "x64"
)

# Provisions the prebuilt ONNX Runtime CPU package (the shared inference runtime
# bundled with the Comparator). Modeled on Ensure-OpenCvDependency.ps1: download
# the official release zip into .deps, verify SHA256, extract include\ + lib\
# into $InstallRoot, and drop a stamp. LPIPS models are NOT provisioned here;
# they are downloaded by the app at runtime (see docs/LPIPS_distribution_design.md).

$ErrorActionPreference = "Stop"

function Test-OrtInstall {
    param([Parameter(Mandatory = $true)][string]$Root)

    $headerPath = Join-Path $Root "include\onnxruntime_c_api.h"
    $dllPath = Join-Path $Root "lib\onnxruntime.dll"
    $stampPath = Join-Path $Root "q1view-onnxruntime.stamp"
    return (Test-Path $headerPath) -and (Test-Path $dllPath) -and (Test-Path $stampPath)
}

function Assert-Sha256 {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [string]$ExpectedHash
    )

    if ([string]::IsNullOrWhiteSpace($ExpectedHash)) {
        Write-Warning "No SHA256 was provided for $FilePath. Set Q1VIEW_ONNXRUNTIME_SHA256 to pin the runtime archive."
        return
    }

    $actualHash = (Get-FileHash -LiteralPath $FilePath -Algorithm SHA256).Hash.ToLowerInvariant()
    $normalizedExpected = $ExpectedHash.Trim().ToLowerInvariant()
    if ($actualHash -ne $normalizedExpected) {
        throw "ONNX Runtime archive SHA256 mismatch. Expected $normalizedExpected, got $actualHash."
    }
}

function Get-ArchiveFileName {
    param([string]$Url)

    if (-not [string]::IsNullOrWhiteSpace($Url)) {
        try {
            $name = [System.IO.Path]::GetFileName(([System.Uri]$Url).LocalPath)
            if (-not [string]::IsNullOrWhiteSpace($name)) { return $name }
        }
        catch {
        }
    }

    return "onnxruntime-win-$Platform-$Version.zip"
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
            throw "ONNX Runtime archive was not found at $resolvedPath"
        }
        Assert-Sha256 -FilePath $resolvedPath -ExpectedHash $ExpectedHash
        return $resolvedPath
    }

    if ([string]::IsNullOrWhiteSpace($Url)) {
        throw "ONNX Runtime archive URL is empty. Set Q1VIEW_ONNXRUNTIME_URL or Q1VIEW_ONNXRUNTIME_ARCHIVE."
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
            Write-Host "Re-downloading ONNX Runtime archive because the cached copy is invalid."
            Remove-Item -LiteralPath $downloadPath -Force
        }
    }

    Write-Host "Downloading ONNX Runtime $Version from $Url"
    Invoke-WebRequest -Uri $Url -OutFile $downloadPath
    Assert-Sha256 -FilePath $downloadPath -ExpectedHash $ExpectedHash
    return $downloadPath
}

function Copy-InstallPayload {
    param(
        [Parameter(Mandatory = $true)][string]$ExtractRoot,
        [Parameter(Mandatory = $true)][string]$InstallRoot
    )

    # The official zip extracts to a single onnxruntime-win-<plat>-<ver>\ folder
    # holding include\ and lib\; accept that nested layout or a flat one.
    $payloadRoot = $ExtractRoot
    if (-not (Test-Path (Join-Path $payloadRoot "include\onnxruntime_c_api.h"))) {
        $nested = Get-ChildItem -LiteralPath $ExtractRoot -Directory |
            Where-Object { Test-Path (Join-Path $_.FullName "include\onnxruntime_c_api.h") } |
            Select-Object -First 1
        if ($nested) { $payloadRoot = $nested.FullName }
    }

    if (-not (Test-Path (Join-Path $payloadRoot "include\onnxruntime_c_api.h")) -or
        -not (Test-Path (Join-Path $payloadRoot "lib\onnxruntime.dll"))) {
        throw "The ONNX Runtime archive does not contain include\onnxruntime_c_api.h and lib\onnxruntime.dll."
    }

    foreach ($sub in @("include", "lib")) {
        Copy-Item -Path (Join-Path $payloadRoot $sub) -Destination $InstallRoot -Recurse -Force
    }
}

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$installRootPath = [System.IO.Path]::GetFullPath($InstallRoot)
$downloadRoot = Join-Path $repoRoot ".deps\downloads"

if (Test-OrtInstall -Root $installRootPath) {
    Write-Host "ONNX Runtime $Version is already installed at $installRootPath"
    exit 0
}

$mutex = New-Object System.Threading.Mutex($false, "Q1ViewEnsureOnnxRuntimeDependency")
$hasLock = $false

try {
    $hasLock = $mutex.WaitOne([TimeSpan]::FromMinutes(20))
    if (-not $hasLock) {
        throw "Timed out waiting for another process to finish preparing ONNX Runtime"
    }

    if (Test-OrtInstall -Root $installRootPath) {
        Write-Host "ONNX Runtime $Version is already installed at $installRootPath"
        exit 0
    }

    $archive = Resolve-Archive -Url $ArchiveUrl -Path $ArchivePath -DownloadRoot $downloadRoot -ExpectedHash $Sha256
    $extractRoot = Join-Path $downloadRoot "onnxruntime-$Version-extract"

    if (Test-Path $extractRoot) {
        Remove-Item -LiteralPath $extractRoot -Recurse -Force
    }
    New-Item -ItemType Directory -Force $extractRoot | Out-Null

    Write-Host "Extracting ONNX Runtime $Version to $installRootPath"
    Expand-Archive -LiteralPath $archive -DestinationPath $extractRoot -Force

    if (Test-Path $installRootPath) {
        Remove-Item -LiteralPath $installRootPath -Recurse -Force
    }
    New-Item -ItemType Directory -Force $installRootPath | Out-Null

    Copy-InstallPayload -ExtractRoot $extractRoot -InstallRoot $installRootPath

    Set-Content -LiteralPath (Join-Path $installRootPath "q1view-onnxruntime.stamp") -Value "$Version`n"

    if (-not (Test-OrtInstall -Root $installRootPath)) {
        throw "ONNX Runtime dependency was extracted, but required files were not found under $installRootPath"
    }

    Write-Host "ONNX Runtime $Version ready at $installRootPath"
}
finally {
    if ($hasLock) {
        $mutex.ReleaseMutex()
    }
    $mutex.Dispose()
}
