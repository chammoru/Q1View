param(
    [string]$Version = "4.3.0",

    [string]$Toolset = "v142",

    [Parameter(Mandatory = $true)]
    [string]$InstallRoot,

    [string]$ArchiveUrl = "",

    [string]$ArchivePath = "",

    [string]$Sha256 = "",

    [string]$Configuration = "Release",

    [string]$Platform = "x64",

    [string]$BinaryPrefix = "x64\vc16"
)

$ErrorActionPreference = "Stop"

function Test-OpenCvInstall {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Root,

        [Parameter(Mandatory = $true)]
        [string]$Config,

        [Parameter(Mandatory = $true)]
        [string]$BinPrefix
    )

    $libSuffix = if ($Config -eq "Debug") { "d" } else { "" }
    $headerPath = Join-Path $Root "include\opencv2\core.hpp"
    $coreLibPath = Join-Path $Root "$BinPrefix\staticlib\opencv_core430$libSuffix.lib"
    $stampPath = Join-Path $Root "q1view-opencv-$Config.stamp"

    return (Test-Path $headerPath) -and (Test-Path $coreLibPath) -and (Test-Path $stampPath)
}

function Assert-Sha256 {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,

        [string]$ExpectedHash
    )

    if ([string]::IsNullOrWhiteSpace($ExpectedHash)) {
        Write-Warning "No SHA256 was provided for $FilePath. Set Q1VIEW_OPENCV_SHA256 after publishing the dependency artifact."
        return
    }

    $actualHash = (Get-FileHash -LiteralPath $FilePath -Algorithm SHA256).Hash.ToLowerInvariant()
    $normalizedExpected = $ExpectedHash.Trim().ToLowerInvariant()
    if ($actualHash -ne $normalizedExpected) {
        throw "OpenCV archive SHA256 mismatch. Expected $normalizedExpected, got $actualHash."
    }
}

function Get-ArchiveFileName {
    param([string]$Url)

    if ([string]::IsNullOrWhiteSpace($Url)) {
        return "Q1View-opencv-$Version-msvc-$Toolset-x64-static-mt.zip"
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

    return "Q1View-opencv-$Version-msvc-$Toolset-x64-static-mt.zip"
}

function Invoke-DownloadWithRetry {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Url,

        [Parameter(Mandatory = $true)]
        [string]$OutFile,

        [string]$Description = "dependency archive",

        [int]$MaxAttempts = 5
    )

    for ($attempt = 1; $attempt -le $MaxAttempts; $attempt++) {
        if (Test-Path $OutFile) {
            Remove-Item -LiteralPath $OutFile -Force
        }

        try {
            Write-Host "Downloading $Description from $Url (attempt $attempt/$MaxAttempts)"
            Invoke-WebRequest -Uri $Url -OutFile $OutFile
            return
        }
        catch {
            if ($attempt -ge $MaxAttempts) {
                throw
            }

            $delaySeconds = [Math]::Min(60, 5 * [Math]::Pow(2, $attempt - 1))
            Write-Warning "Download failed: $($_.Exception.Message)"
            Write-Host "Retrying in $delaySeconds seconds..."
            Start-Sleep -Seconds $delaySeconds
        }
    }
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
            throw "OpenCV archive was not found at $resolvedPath"
        }

        Assert-Sha256 -FilePath $resolvedPath -ExpectedHash $ExpectedHash
        return $resolvedPath
    }

    if ([string]::IsNullOrWhiteSpace($Url)) {
        throw "OpenCV archive URL is empty. Set Q1VIEW_OPENCV_URL or Q1VIEW_OPENCV_ARCHIVE."
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
            Write-Host "Re-downloading OpenCV archive because the cached copy is invalid."
            Remove-Item -LiteralPath $downloadPath -Force
        }
    }

    Invoke-DownloadWithRetry -Url $Url -OutFile $downloadPath -Description "OpenCV $Version dependency"
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
    $hasQ1ViewStamp = @(Get-ChildItem -LiteralPath $Root -Filter "q1view-opencv-*.stamp" -ErrorAction SilentlyContinue).Count -gt 0

    if ((-not $insideDeps) -and (-not $hasQ1ViewStamp)) {
        throw "Refusing to replace incomplete custom OpenCV root: $fullRoot"
    }

    Remove-Item -LiteralPath $Root -Recurse -Force
    New-Item -ItemType Directory -Force $Root | Out-Null
}

function Copy-InstallPayload {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ExtractRoot,

        [Parameter(Mandatory = $true)]
        [string]$InstallRoot
    )

    $payloadRoot = $ExtractRoot
    $nestedInstall = Join-Path $ExtractRoot "install"
    if (Test-Path (Join-Path $nestedInstall "include\opencv2\core.hpp")) {
        $payloadRoot = $nestedInstall
    }

    if (-not (Test-Path (Join-Path $payloadRoot "include\opencv2\core.hpp"))) {
        throw "The OpenCV archive does not contain include\opencv2\core.hpp at its root or under install\."
    }

    Copy-Item -Path (Join-Path $payloadRoot "*") -Destination $InstallRoot -Recurse -Force
}

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$installRootPath = [System.IO.Path]::GetFullPath($InstallRoot)
$downloadRoot = Join-Path $repoRoot ".deps\downloads"

if (Test-OpenCvInstall -Root $installRootPath -Config $Configuration -BinPrefix $BinaryPrefix) {
    Write-Host "OpenCV $Version is already installed at $installRootPath"
    exit 0
}

$mutex = New-Object System.Threading.Mutex($false, "Q1ViewEnsureOpenCvDependency")
$hasLock = $false

try {
    $hasLock = $mutex.WaitOne([TimeSpan]::FromMinutes(20))
    if (-not $hasLock) {
        throw "Timed out waiting for another process to finish preparing OpenCV"
    }

    if (Test-OpenCvInstall -Root $installRootPath -Config $Configuration -BinPrefix $BinaryPrefix) {
        Write-Host "OpenCV $Version is already installed at $installRootPath"
        exit 0
    }

    $archive = Resolve-Archive -Url $ArchiveUrl -Path $ArchivePath -DownloadRoot $downloadRoot -ExpectedHash $Sha256
    $extractRoot = Join-Path $downloadRoot "opencv-$Version-prebuilt-extract"

    if (Test-Path $extractRoot) {
        Remove-Item -LiteralPath $extractRoot -Recurse -Force
    }
    New-Item -ItemType Directory -Force $extractRoot | Out-Null

    Write-Host "Extracting OpenCV $Version dependency to $installRootPath"
    Expand-Archive -LiteralPath $archive -DestinationPath $extractRoot -Force

    Clear-InstallRoot -Root $installRootPath -RepoRoot $repoRoot
    Copy-InstallPayload -ExtractRoot $extractRoot -InstallRoot $installRootPath

    if (-not (Test-OpenCvInstall -Root $installRootPath -Config $Configuration -BinPrefix $BinaryPrefix)) {
        throw "OpenCV dependency was extracted, but required $Configuration files were not found under $installRootPath"
    }
}
finally {
    if ($hasLock) {
        $mutex.ReleaseMutex()
    }
    $mutex.Dispose()
}
