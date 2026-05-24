param(
    [Parameter(Mandatory = $true)]
    [string[]]$Files,

    [string]$CertificatePath = $env:Q1VIEW_SIGN_CERT_PATH,

    [string]$CertificatePassword = $env:Q1VIEW_SIGN_CERT_PASSWORD,

    [string]$CertificateBase64 = $env:Q1VIEW_SIGN_CERT_BASE64,

    [string]$CertificateThumbprint = $env:Q1VIEW_SIGN_CERT_THUMBPRINT,

    [string]$TimestampUrl = "http://timestamp.digicert.com",

    [switch]$Required
)

$ErrorActionPreference = "Stop"

function Find-SignTool {
    $command = Get-Command "signtool.exe" -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $kitRoots = @(
        "${env:ProgramFiles(x86)}\Windows Kits\10\bin",
        "$env:ProgramFiles\Windows Kits\10\bin"
    )

    foreach ($kitRoot in $kitRoots) {
        if ([string]::IsNullOrWhiteSpace($kitRoot) -or -not (Test-Path $kitRoot)) {
            continue
        }

        $candidate = Get-ChildItem -LiteralPath $kitRoot -Recurse -Filter "signtool.exe" -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match "\\x64\\signtool\.exe$" } |
            Sort-Object FullName -Descending |
            Select-Object -First 1
        if ($candidate) {
            return $candidate.FullName
        }
    }

    return ""
}

$signTool = Find-SignTool
if ([string]::IsNullOrWhiteSpace($signTool)) {
    if ($Required) {
        throw "signtool.exe was not found."
    }

    Write-Warning "signtool.exe was not found. Skipping Authenticode signing."
    exit 0
}

$tempCertificatePath = ""
if ([string]::IsNullOrWhiteSpace($CertificatePath) -and -not [string]::IsNullOrWhiteSpace($CertificateBase64)) {
    $tempCertificatePath = Join-Path ([System.IO.Path]::GetTempPath()) ("q1view-signing-{0}.pfx" -f ([System.Guid]::NewGuid()))
    [System.IO.File]::WriteAllBytes($tempCertificatePath, [Convert]::FromBase64String($CertificateBase64))
    $CertificatePath = $tempCertificatePath
}

try {
    $hasPfx = -not [string]::IsNullOrWhiteSpace($CertificatePath)
    $hasThumbprint = -not [string]::IsNullOrWhiteSpace($CertificateThumbprint)

    if (-not $hasPfx -and -not $hasThumbprint) {
        if ($Required) {
            throw "No signing certificate was provided. Set Q1VIEW_SIGN_CERT_BASE64, Q1VIEW_SIGN_CERT_PATH, or Q1VIEW_SIGN_CERT_THUMBPRINT."
        }

        Write-Warning "No signing certificate was provided. Skipping Authenticode signing."
        exit 0
    }

    $existingFiles = @()
    foreach ($file in $Files) {
        $resolved = [System.IO.Path]::GetFullPath($file)
        if (-not (Test-Path $resolved)) {
            throw "File to sign was not found: $resolved"
        }
        $existingFiles += $resolved
    }

    foreach ($file in $existingFiles) {
        $arguments = @("sign", "/fd", "SHA256", "/tr", $TimestampUrl, "/td", "SHA256")
        if ($hasPfx) {
            $arguments += @("/f", $CertificatePath)
            if (-not [string]::IsNullOrWhiteSpace($CertificatePassword)) {
                $arguments += @("/p", $CertificatePassword)
            }
        } else {
            $arguments += @("/sha1", $CertificateThumbprint)
        }
        $arguments += $file

        Write-Host "Signing $file"
        & $signTool @arguments
        if ($LASTEXITCODE -ne 0) {
            throw "signtool.exe failed with exit code $LASTEXITCODE"
        }
    }
}
finally {
    if (-not [string]::IsNullOrWhiteSpace($tempCertificatePath) -and (Test-Path $tempCertificatePath)) {
        Remove-Item -LiteralPath $tempCertificatePath -Force
    }
}
