param(
    # Packaged Q1View directory produced by Package-Q1View.ps1.
    [string]$SourceDir = "",

    # Directory where Q1View-windows-x64.msix will be written.
    [string]$OutputDir = "",

    # Package version in X.X.X.X format. Replaces the placeholder in AppxManifest.xml.
    [string]$AppVersion = "1.0.0.0",

    # Publisher CN. Must match the signing certificate Subject exactly.
    # For Microsoft Store: use the Publisher value shown in Partner Center.
    # For sideloading: use any CN that matches your self-signed certificate.
    [string]$Publisher = "CN=Q1ViewPublisher",

    # Path to the Assets directory containing required PNG logo files.
    [string]$AssetsDir = "",

    # Path to the AppxManifest.xml template.
    [string]$ManifestTemplate = "",

    # Signing inputs. Defaults to the same environment variables used by Sign-Q1View.ps1.
    [string]$CertificatePath      = $env:Q1VIEW_SIGN_CERT_PATH,
    [string]$CertificatePassword  = $env:Q1VIEW_SIGN_CERT_PASSWORD,
    [string]$CertificateBase64    = $env:Q1VIEW_SIGN_CERT_BASE64,
    [string]$CertificateThumbprint = $env:Q1VIEW_SIGN_CERT_THUMBPRINT,
    [string]$TimestampUrl         = "http://timestamp.digicert.com",

    # Skip Authenticode signing (useful for manifest validation without a certificate).
    [switch]$SkipSigning
)

$ErrorActionPreference = "Stop"

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))

if ([string]::IsNullOrWhiteSpace($SourceDir)) {
    $SourceDir = Join-Path $repoRoot "dist\Q1View-windows-x64"
}
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $repoRoot "dist"
}
if ([string]::IsNullOrWhiteSpace($AssetsDir)) {
    $AssetsDir = Join-Path $repoRoot "installer\msix\Assets"
}
if ([string]::IsNullOrWhiteSpace($ManifestTemplate)) {
    $ManifestTemplate = Join-Path $repoRoot "installer\msix\AppxManifest.xml"
}

# --- Validate inputs --------------------------------------------------------

if (-not (Test-Path $SourceDir)) {
    throw "SourceDir not found: $SourceDir`n  Build the applications first with Package-Q1View.ps1."
}
foreach ($exe in @("Viewer.exe", "Comparer.exe")) {
    if (-not (Test-Path (Join-Path $SourceDir $exe))) {
        throw "Missing required binary in SourceDir: $exe"
    }
}

if (-not (Test-Path $ManifestTemplate)) {
    throw "Manifest template not found: $ManifestTemplate"
}

if (-not (Test-Path $AssetsDir)) {
    throw "Assets directory not found: $AssetsDir`n  See installer\msix\Assets\README.md for required files."
}
$requiredAssets = @("Square44x44Logo.png", "Square150x150Logo.png", "Wide310x150Logo.png", "StoreLogo.png")
foreach ($asset in $requiredAssets) {
    if (-not (Test-Path (Join-Path $AssetsDir $asset))) {
        throw "Missing required asset: $asset`n  See installer\msix\Assets\README.md."
    }
}

if ($AppVersion -notmatch '^\d+\.\d+\.\d+\.\d+$') {
    throw "AppVersion must be in X.X.X.X format (e.g. 1.0.16.0), got: $AppVersion"
}

# --- Find makeappx.exe ------------------------------------------------------

function Find-MakeAppx {
    $cmd = Get-Command "makeappx.exe" -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    foreach ($kitRoot in @(
        "${env:ProgramFiles(x86)}\Windows Kits\10\bin",
        "$env:ProgramFiles\Windows Kits\10\bin"
    )) {
        if ([string]::IsNullOrWhiteSpace($kitRoot) -or -not (Test-Path $kitRoot)) { continue }
        $found = Get-ChildItem -LiteralPath $kitRoot -Recurse -Filter "makeappx.exe" `
            -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match "\\x64\\makeappx\.exe$" } |
            Sort-Object FullName -Descending |
            Select-Object -First 1
        if ($found) { return $found.FullName }
    }
    return ""
}

$makeAppx = Find-MakeAppx
if ([string]::IsNullOrWhiteSpace($makeAppx)) {
    throw "makeappx.exe not found. Install the Windows 10 SDK (available via Visual Studio Installer or winget)."
}
Write-Host "Using makeappx: $makeAppx"

# --- Build staging layout ---------------------------------------------------

$stagingDir = Join-Path $repoRoot "dist\_msix-staging"
if (Test-Path $stagingDir) {
    Remove-Item -LiteralPath $stagingDir -Recurse -Force
}
New-Item -ItemType Directory -Force $stagingDir | Out-Null
New-Item -ItemType Directory -Force (Join-Path $stagingDir "Assets") | Out-Null

# Application binaries and runtime DLLs
Get-ChildItem -LiteralPath $SourceDir -File |
    Where-Object { $_.Extension -in @(".exe", ".dll") } |
    ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $stagingDir -Force
    }

# Logo assets
foreach ($asset in $requiredAssets) {
    Copy-Item -LiteralPath (Join-Path $AssetsDir $asset) `
              -Destination (Join-Path $stagingDir "Assets") -Force
}

# Optional scale variants (copy any that exist alongside the required files)
Get-ChildItem -LiteralPath $AssetsDir -Filter "*.scale-*.png" -ErrorAction SilentlyContinue |
    ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $stagingDir "Assets") -Force
    }

# Manifest — set Publisher and Version via XML DOM
[xml]$manifest = Get-Content -LiteralPath $ManifestTemplate -Encoding utf8
$manifest.Package.Identity.Publisher = $Publisher
$manifest.Package.Identity.Version   = $AppVersion

$manifestDest = Join-Path $stagingDir "AppxManifest.xml"
$settings = New-Object System.Xml.XmlWriterSettings
$settings.Indent = $true
$settings.IndentChars = "  "
$settings.Encoding = [System.Text.Encoding]::UTF8
$writer = [System.Xml.XmlWriter]::Create($manifestDest, $settings)
$manifest.Save($writer)
$writer.Close()

Write-Host "Staging layout:"
Get-ChildItem -LiteralPath $stagingDir -Recurse -File |
    Sort-Object FullName |
    ForEach-Object { Write-Host ("  {0}" -f $_.FullName.Substring($stagingDir.Length + 1)) }

# --- Create MSIX ------------------------------------------------------------

New-Item -ItemType Directory -Force $OutputDir | Out-Null
$msixPath = Join-Path ([System.IO.Path]::GetFullPath($OutputDir)) "Q1View-windows-x64.msix"
if (Test-Path $msixPath) {
    Remove-Item -LiteralPath $msixPath -Force
}

Write-Host ""
Write-Host "Building MSIX package..."
& $makeAppx pack /d $stagingDir /p $msixPath
if ($LASTEXITCODE -ne 0) {
    throw "makeappx.exe failed with exit code $LASTEXITCODE"
}

Remove-Item -LiteralPath $stagingDir -Recurse -Force

# --- Sign -------------------------------------------------------------------

if ($SkipSigning) {
    Write-Host "Skipping signing (-SkipSigning was set)."
} else {
    Write-Host "Signing MSIX package..."
    & (Join-Path $repoRoot "build\Sign-Q1View.ps1") `
        -Files @($msixPath) `
        -CertificatePath       $CertificatePath `
        -CertificatePassword   $CertificatePassword `
        -CertificateBase64     $CertificateBase64 `
        -CertificateThumbprint $CertificateThumbprint `
        -TimestampUrl          $TimestampUrl `
        -Required
}

Write-Host ""
Write-Host "Created MSIX package: $msixPath"
