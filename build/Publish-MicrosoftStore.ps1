param(
    [Parameter(Mandatory = $true)]
    [string]$ProductId,

    [Parameter(Mandatory = $true)]
    [string]$PackagePath,

    [string]$WhatsNewPath = "STORE_WHATS_NEW.txt",
    [string]$StoreListingPath = "docs/STORE_LISTING.md"
)

$ErrorActionPreference = "Stop"

function Invoke-MsStoreCommand {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Description,

        [Parameter(Mandatory = $true)]
        [string[]]$Arguments,

        [int]$Attempts = 1,
        [int]$InitialRetryDelaySeconds = 30,
        [switch]$CaptureOutput,
        [switch]$AllowFailure
    )

    for ($attempt = 1; $attempt -le $Attempts; $attempt++) {
        Write-Host "::group::$Description (attempt $attempt/$Attempts)"
        $commandOutput = @(& msstore @Arguments)
        $exitCode = $LASTEXITCODE
        if (-not $CaptureOutput -or $exitCode -ne 0) {
            $commandOutput | ForEach-Object { Write-Host $_ }
        }
        Write-Host "::endgroup::"

        if ($exitCode -eq 0) {
            return [pscustomobject]@{
                Succeeded = $true
                ExitCode = $exitCode
                Output = if ($CaptureOutput) { $commandOutput -join "`n" } else { "" }
            }
        }

        if ($attempt -lt $Attempts) {
            $delay = [Math]::Min(180, $InitialRetryDelaySeconds * $attempt)
            Write-Host "::warning::$Description failed with exit code $exitCode; retrying in $delay seconds."
            Start-Sleep -Seconds $delay
        }
    }

    if ($AllowFailure) {
        Write-Host "::warning::$Description failed after $Attempts attempt(s); continuing."
        return [pscustomobject]@{
            Succeeded = $false
            ExitCode = $exitCode
            Output = if ($CaptureOutput) { $commandOutput -join "`n" } else { "" }
        }
    }

    throw "$Description failed after $Attempts attempt(s)."
}

function Remove-PendingStoreSubmission {
    $null = Invoke-MsStoreCommand `
        -Description "Delete pending Microsoft Store submission" `
        -Arguments @("submission", "delete", $ProductId, "--no-confirm", "--verbose") `
        -Attempts 2 `
        -AllowFailure
}

function Stage-StorePackage {
    $null = Invoke-MsStoreCommand `
        -Description "Stage Microsoft Store package" `
        -Arguments @("publish", $PackagePath, "-id", $ProductId, "--noCommit", "--verbose") `
        -Attempts 4
}

function Publish-StoreSubmission {
    $null = Invoke-MsStoreCommand `
        -Description "Publish Microsoft Store submission" `
        -Arguments @("submission", "publish", $ProductId, "--verbose") `
        -Attempts 4
}

function Get-StoreListingBlock {
    param([Parameter(Mandatory = $true)][string]$HeadingPattern)

    if (-not (Test-Path $StoreListingPath)) {
        return ""
    }

    $lines = Get-Content $StoreListingPath -Encoding utf8
    $inSection = $false
    $inFence = $false
    $block = New-Object System.Collections.Generic.List[string]

    foreach ($line in $lines) {
        if (-not $inSection) {
            if ($line -match $HeadingPattern) {
                $inSection = $true
            }
            continue
        }
        if ($line -match '^```') {
            if ($inFence) {
                break
            }
            $inFence = $true
            continue
        }
        if ($inFence) {
            $block.Add($line)
        }
    }

    return (($block -join "`n").Trim())
}

function Set-ListingText {
    param(
        $Node,
        [string]$ReleaseNotes,
        [string]$Description,
        [string]$ShortDescription
    )

    if ($null -eq $Node) {
        return
    }

    if ($Node -is [pscustomobject]) {
        foreach ($property in $Node.PSObject.Properties) {
            if ($property.Name -eq "releaseNotes") {
                $property.Value = $ReleaseNotes
                $script:ReleaseNotesUpdated++
            } elseif ($Description -and $property.Name -eq "description") {
                $property.Value = $Description
                $script:DescriptionUpdated++
            } elseif ($ShortDescription -and $property.Name -eq "shortDescription") {
                $property.Value = $ShortDescription
                $script:ShortDescriptionUpdated++
            } else {
                Set-ListingText $property.Value $ReleaseNotes $Description $ShortDescription
            }
        }
    } elseif ($Node -is [System.Collections.IEnumerable] -and $Node -isnot [string]) {
        foreach ($item in $Node) {
            Set-ListingText $item $ReleaseNotes $Description $ShortDescription
        }
    }
}

function Update-StoreMetadata {
    if (-not (Test-Path $WhatsNewPath)) {
        Write-Host "::warning::$WhatsNewPath not found; Store What's New left unchanged."
        return $true
    }

    $notes = (Get-Content $WhatsNewPath -Raw).Trim()
    if (-not $notes) {
        Write-Host "::warning::Store What's New is empty; Store metadata left unchanged."
        return $true
    }

    $storeDescription = Get-StoreListingBlock '^## Description '
    $storeShortDescription = Get-StoreListingBlock '^## Short description '

    $getResult = Invoke-MsStoreCommand `
        -Description "Get Microsoft Store submission metadata" `
        -Arguments @("submission", "get", $ProductId, "--verbose") `
        -Attempts 4 `
        -CaptureOutput
    $raw = $getResult.Output

    $open = $raw.IndexOf('{')
    $close = $raw.LastIndexOf('}')
    if ($open -lt 0 -or $close -lt $open) {
        throw "submission get returned no JSON object"
    }

    $json = $raw.Substring($open, $close - $open + 1) | ConvertFrom-Json
    $script:ReleaseNotesUpdated = 0
    $script:DescriptionUpdated = 0
    $script:ShortDescriptionUpdated = 0
    Set-ListingText $json $notes $storeDescription $storeShortDescription

    if ($script:ReleaseNotesUpdated -le 0) {
        Write-Host "::warning::No 'releaseNotes' field found in submission metadata; What's New left unchanged."
        return $true
    }

    $metadata = $json | ConvertTo-Json -Depth 50 -Compress
    $updateResult = Invoke-MsStoreCommand `
        -Description "Update Microsoft Store metadata" `
        -Arguments @("submission", "updateMetadata", $ProductId, $metadata, "--verbose") `
        -Attempts 3 `
        -AllowFailure

    if ($updateResult.Succeeded) {
        Write-Host "Updated What's New in $($script:ReleaseNotesUpdated) listing(s)."
        Write-Host "Updated Store description in $($script:DescriptionUpdated) listing(s)."
        Write-Host "Updated Store short description in $($script:ShortDescriptionUpdated) listing(s)."
    }

    return $updateResult.Succeeded
}

function Update-StoreMetadataBestEffort {
    param([Parameter(Mandatory = $true)][string]$Context)

    try {
        if (-not (Update-StoreMetadata)) {
            Write-Host "::warning::Store metadata update failed $Context; continuing without it."
        }
    } catch {
        Write-Host "::warning::Store metadata update failed $Context; continuing without it: $($_.Exception.Message)"
    }
}

try {
    Stage-StorePackage
} catch {
    Write-Host "::warning::Initial package staging failed: $($_.Exception.Message)"
    Remove-PendingStoreSubmission
    Stage-StorePackage
}

$metadataUpdated = $false
try {
    $metadataUpdated = Update-StoreMetadata
} catch {
    Write-Host "::warning::Store metadata update failed: $($_.Exception.Message)"
}

if (-not $metadataUpdated) {
    Write-Host "::warning::Store metadata update failed; restaging the package before submission."
    Remove-PendingStoreSubmission
    Stage-StorePackage
    Update-StoreMetadataBestEffort -Context "after restaging the package"
}

try {
    Publish-StoreSubmission
} catch {
    Write-Host "::warning::Submission publish failed: $($_.Exception.Message)"
    try {
        Remove-PendingStoreSubmission
        Stage-StorePackage
        Update-StoreMetadataBestEffort -Context "after restaging the failed submission"
        Publish-StoreSubmission
    } catch {
        throw "Microsoft Store submission recovery failed. The Store may already have accepted an earlier timed-out request; check the submission status in Partner Center before rerunning this workflow. Last error: $($_.Exception.Message)"
    }
}
