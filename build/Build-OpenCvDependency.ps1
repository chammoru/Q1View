param(
    [string]$Version = "4.3.0",

    [string]$Toolset = "v142",

    [string]$SourceRoot = "",

    [string]$BuildRoot = "",

    [string]$InstallRoot = "",

    [string[]]$Configurations = @("Release", "Debug"),

    [string]$Platform = "x64"
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

function Get-CMake {
    $cmake = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmake) {
        return $cmake.Source
    }

    throw "CMake was not found in PATH. Install CMake or use a Visual Studio environment that provides it."
}

function Expand-Zip {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ZipPath,

        [Parameter(Mandatory = $true)]
        [string]$Destination
    )

    if (Test-Path $Destination) {
        Remove-Item -LiteralPath $Destination -Recurse -Force
    }

    New-Item -ItemType Directory -Force $Destination | Out-Null
    Expand-Archive -LiteralPath $ZipPath -DestinationPath $Destination -Force
}

function Test-InstalledConfiguration {
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

    return (Test-Path $headerPath) -and (Test-Path $coreLibPath)
}

function Repair-OpenCvSource {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Root
    )

    $attributeSourcePath = Join-Path $Root "3rdparty\openexr\IlmImf\ImfAttribute.cpp"
    if (-not (Test-Path $attributeSourcePath)) {
        return
    }

    $attributeSource = Get-Content -LiteralPath $attributeSourcePath -Raw
    if ($attributeSource -match "#include\s+<functional>") {
        return
    }

    Write-Host "Patching bundled OpenEXR include for current MSVC STL"
    if ($attributeSource.Contains("#include <ImfAttribute.h>")) {
        $attributeSource = $attributeSource.Replace(
            "#include <ImfAttribute.h>",
            "#include <ImfAttribute.h>`r`n#include <functional>"
        )
    } elseif ($attributeSource.Contains("#include `"ImfAttribute.h`"")) {
        $attributeSource = $attributeSource.Replace(
            "#include `"ImfAttribute.h`"",
            "#include `"ImfAttribute.h`"`r`n#include <functional>"
        )
    } else {
        throw "Could not find ImfAttribute include to patch in $attributeSourcePath"
    }

    Set-Content -LiteralPath $attributeSourcePath -Value $attributeSource -NoNewline
}

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
if ([string]::IsNullOrWhiteSpace($SourceRoot)) {
    $SourceRoot = Join-Path $repoRoot ".deps\opencv-$Version"
}
if ([string]::IsNullOrWhiteSpace($BuildRoot)) {
    $BuildRoot = Join-Path $repoRoot ".deps\opencv-$Version-build"
}
if ([string]::IsNullOrWhiteSpace($InstallRoot)) {
    $InstallRoot = Join-Path $BuildRoot "install"
}

$sourceRootPath = [System.IO.Path]::GetFullPath($SourceRoot)
$buildRootPath = [System.IO.Path]::GetFullPath($BuildRoot)
$installRootPath = [System.IO.Path]::GetFullPath($InstallRoot)
$platformName = if ($Platform -eq "x64") { "x64" } else { $Platform }
$binaryPrefix = "$platformName\vc16"

$depsRoot = Join-Path $repoRoot ".deps"
New-Item -ItemType Directory -Force $depsRoot | Out-Null

if (-not (Test-Path (Join-Path $sourceRootPath "CMakeLists.txt"))) {
    $downloadRoot = Join-Path $depsRoot "downloads"
    New-Item -ItemType Directory -Force $downloadRoot | Out-Null
    $zipPath = Join-Path $downloadRoot "opencv-$Version.zip"
    $downloadUrl = "https://github.com/opencv/opencv/archive/$Version.zip"

    if (-not (Test-Path $zipPath)) {
        Write-Host "Downloading OpenCV $Version from $downloadUrl"
        Invoke-WebRequest -Uri $downloadUrl -OutFile $zipPath
    }

    $extractRoot = Join-Path $downloadRoot "opencv-$Version-source-extract"
    Write-Host "Extracting OpenCV $Version source"
    Expand-Zip -ZipPath $zipPath -Destination $extractRoot

    $extractedSource = Join-Path $extractRoot "opencv-$Version"
    if (-not (Test-Path (Join-Path $extractedSource "CMakeLists.txt"))) {
        throw "Downloaded OpenCV source was not found at $extractedSource"
    }

    if (Test-Path $sourceRootPath) {
        Remove-Item -LiteralPath $sourceRootPath -Recurse -Force
    }

    Move-Item -LiteralPath $extractedSource -Destination $sourceRootPath
}

Repair-OpenCvSource -Root $sourceRootPath

$cmake = Get-CMake
$configureStamp = Join-Path $buildRootPath "CMakeCache.txt"
if (Test-Path $configureStamp) {
    $cacheText = Get-Content -LiteralPath $configureStamp -Raw
    $toolsetPattern = "CMAKE_GENERATOR_TOOLSET:INTERNAL=$([regex]::Escape($Toolset))(\r?\n|,)"
    if ($cacheText -notmatch "BUILD_WITH_STATIC_CRT:BOOL=ON" -or
            $cacheText -notmatch $toolsetPattern) {
        Write-Host "Reconfiguring OpenCV because the existing build tree does not use $Toolset with the static CRT"
        Remove-Item -LiteralPath $buildRootPath -Recurse -Force
        if (Test-Path $installRootPath) {
            Remove-Item -LiteralPath $installRootPath -Recurse -Force
        }
    }
}

if (-not (Test-Path $configureStamp)) {
    New-Item -ItemType Directory -Force $buildRootPath | Out-Null

    Write-Host "Configuring OpenCV $Version"
    Invoke-Checked -FilePath $cmake -Arguments @(
        "-S", $sourceRootPath,
        "-B", $buildRootPath,
        "-A", $platformName,
        "-T", $Toolset,
        "-DOpenCV_ARCH=$platformName",
        "-DOpenCV_RUNTIME=vc16",
        "-DCMAKE_INSTALL_PREFIX=$installRootPath",
        "-DCMAKE_CXX_STANDARD:STRING=14",
        "-DCMAKE_CXX_STANDARD_REQUIRED:BOOL=ON",
        "-DBUILD_DOCS:BOOL=OFF",
        "-DBUILD_opencv_apps:BOOL=OFF",
        "-DWITH_CUDA:BOOL=OFF",
        "-DBUILD_EXAMPLES:BOOL=OFF",
        "-DWITH_GSTREAMER:BOOL=OFF",
        "-DWITH_OPENCLAMDBLAS:BOOL=OFF",
        "-DBUILD_PERF_TESTS:BOOL=OFF",
        "-DWITH_OPENCLAMDFFT:BOOL=OFF",
        "-DBUILD_SHARED_LIBS:BOOL=OFF",
        "-DBUILD_TESTS:BOOL=OFF",
        "-DWITH_OPENCL_D3D11_NV:BOOL=OFF",
        "-DBUILD_PACKAGE:BOOL=OFF",
        "-DWITH_OPENCL:BOOL=OFF",
        "-DOPENCV_DNN_OPENCL:BOOL=OFF",
        "-DBUILD_opencv_dnn:BOOL=OFF",
        "-DWITH_PROTOBUF:BOOL=OFF",
        "-DBUILD_PROTOBUF:BOOL=OFF",
        "-DWITH_QUIRC:BOOL=OFF",
        "-DBUILD_WITH_STATIC_CRT:BOOL=ON",
        "-DWITH_DIRECTX:BOOL=OFF",
        "-DWITH_DSHOW:BOOL=OFF",
        "-DWITH_MSMF:BOOL=OFF",
        "-DWITH_MSMF_DXVA:BOOL=OFF",
        "-DWITH_FFMPEG:BOOL=ON",
        "-DBUILD_LIST=core,imgproc,imgcodecs,videoio,highgui"
    )
}

foreach ($configuration in $Configurations) {
    Write-Host "Building and installing OpenCV $Version ($configuration|$platformName)"
    $buildTargets = @(
        "ade",
        "opencv_core",
        "opencv_imgproc",
        "opencv_imgcodecs",
        "opencv_videoio",
        "opencv_highgui"
    )
    $buildArguments = @(
        "--build", $buildRootPath,
        "--config", $configuration,
        "--target"
    ) + $buildTargets + @(
        "--", "/m"
    )
    Invoke-Checked -FilePath $cmake -Arguments $buildArguments

    Invoke-Checked -FilePath $cmake -Arguments @(
        "--install", $buildRootPath,
        "--config", $configuration
    )

    if (-not (Test-InstalledConfiguration -Root $installRootPath -Config $configuration -BinPrefix $binaryPrefix)) {
        throw "OpenCV $configuration build completed, but required files were not found under $installRootPath"
    }

    $stampPath = Join-Path $installRootPath "q1view-opencv-$configuration.stamp"
    "OpenCV $Version $configuration $platformName $Toolset static-crt" | Out-File -LiteralPath $stampPath -Encoding ascii
}

$ffmpegDllPath = Join-Path $installRootPath "$binaryPrefix\bin\opencv_videoio_ffmpeg430_64.dll"
if (-not (Test-Path $ffmpegDllPath)) {
    Write-Warning "OpenCV FFmpeg DLL was not found at $ffmpegDllPath"
}
