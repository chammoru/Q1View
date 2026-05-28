# Microsoft Store Submission Guide

This document covers the end-to-end process for submitting Q1View to the
Microsoft Store, from account setup through certification.

## Prerequisites

Before starting a submission, confirm the following are ready:

| Item | Status | Notes |
| --- | --- | --- |
| Microsoft Partner Center account | Required | partner.microsoft.com — one-time setup |
| Code-signing certificate | Required | EV certificate recommended; must match Publisher CN in AppxManifest |
| MSIX package builds cleanly | Required | See [MSIX Packaging](#msix-packaging) below |
| Logo assets (all required sizes) | Required | See `installer/msix/Assets/README.md` |
| Privacy Policy URL | Ready | `https://github.com/chammoru/Q1View/blob/master/PRIVACY.md` |
| Store listing text | Ready | See `docs/STORE_LISTING.md` |
| Store screenshots | Required | Show both Viewer and Comparator workflows; see [Screenshots](#screenshots) below |

## MSIX Packaging

Build both applications, then create the MSIX:

```powershell
# 1. Build
msbuild Viewer\Viewer.sln /m /restore /p:Configuration=Release /p:Platform=x64
msbuild Comparator\Comparator.sln /m /restore /p:Configuration=Release /p:Platform=x64

# 2. Stage binaries
.\build\Package-Q1View.ps1 -Configuration Release -Platform x64

# 3. Create and sign MSIX
#    Publisher must exactly match your Partner Center certificate Subject.
.\build\Package-Q1ViewMsix.ps1 `
    -AppVersion 1.0.16.0 `
    -Publisher "CN=A89D24B3-A271-4AE1-9B9E-BFAE414EB0C6"
```

The signed package is written to `dist\Q1View-windows-x64.msix`.

### Validate the package locally

```powershell
# Install (sideload)
Add-AppPackage -Path .\dist\Q1View-windows-x64.msix

# Verify Viewer launches and its Compare action opens Comparator, then uninstall
Get-AppPackage -Name "KyuwonKim.Q1View" | Remove-AppPackage
```

### Run Windows App Certification Kit (WACK)

Before submitting, run WACK against the installed package:

1. Install the package (step above).
2. Open **Windows App Certification Kit** (installed with the Windows SDK).
3. Select **Validate a Store app**.
4. Select the installed Q1View package.
5. Review the report and resolve any failures before submitting.

Common WACK checks relevant to Q1View:
- Supported APIs (no deprecated or unsupported Win32 calls)
- Digitally signed binaries
- Correct manifest declarations

## Partner Center Setup (one-time)

1. Go to [partner.microsoft.com](https://partner.microsoft.com) and sign in with
   a Microsoft account.
2. Enroll in the **Windows** program (individual or company account).
3. In **Account settings → Organization profile**, note the exact
   **Publisher display name** — this becomes the `Publisher` CN value.
4. Reserve the app name **Q1View** under **Windows & Xbox → Overview → New product**.

## Submission Checklist

### 1. App identity and pricing

- [ ] App name: **Q1View**
- [ ] Pricing: Free
- [ ] Markets: All markets (or select as appropriate)
- [ ] Store listing language: English (en-US)

### 2. Properties

- [ ] Category: **Developer tools** (primary)
- [ ] Sub-category: **Utilities**
- [ ] Privacy Policy URL: `https://github.com/chammoru/Q1View/blob/master/PRIVACY.md`
- [ ] Support URL: `https://github.com/chammoru/Q1View/issues`
- [ ] Website: `https://github.com/chammoru/Q1View`

### 3. Age ratings

Complete the ESRB questionnaire in Partner Center. Expected outcome:
- **ESRB: Everyone (E)** — no violence, no adult content, no user interaction features.
- Answer "No" to all content questions (no user-generated content, no location
  data, no in-app purchases, no advertising, no communication features).

### 4. Store listing

Use the text from `docs/STORE_LISTING.md`:

- [ ] Description (up to 10,000 characters)
- [ ] Short description (up to 256 characters)
- [ ] Feature list (up to 20 items, 200 characters each)
- [ ] Search terms (up to 7 terms, 30 characters each)
- [ ] Release notes

### 5. Screenshots

At minimum, provide screenshots that show the Viewer and Comparator workflows.
The Store package exposes Viewer as its single entry point; Comparator is
included in the package and launched from Viewer's **Compare** action.
Recommended set:

| Screenshot | Resolution | Content |
| --- | --- | --- |
| Viewer — video frame | 1920 × 1080 | Video playback with seek bar visible |
| Viewer — pixel inspection | 1920 × 1080 | High-zoom view showing pixel values |
| Viewer — Sync Input | 1920 × 1080 | Two Viewer windows synchronized |
| Comparator — PSNR | 1920 × 1080 | Side-by-side with PSNR graph |
| Comparator — SSIM | 1920 × 1080 | Side-by-side with SSIM graph |

Accepted sizes: 1366×768 minimum, 3840×2160 maximum. PNG or JPEG.

### 6. Packages

- [ ] Upload `dist\Q1View-windows-x64.msix`
- [ ] Confirm package details in Partner Center match the manifest:
  - Publisher CN matches certificate
  - Version matches the release tag (e.g. `1.0.16.0`)
  - Viewer appears as the Store entry point and can launch the included Comparator workflow

### 7. Final review and submit

- [ ] Preview the Store listing in Partner Center
- [ ] Review all checklist items above
- [ ] Click **Submit to the Store**

## Certification and Publication

Microsoft certification typically takes 1–3 business days. Common rejection
reasons to avoid:

- WACK failures (run WACK locally before submitting)
- Inaccurate age rating answers
- Privacy Policy URL unreachable or missing data collection disclosures
- Screenshots that do not match the submitted package version

If certification fails, Partner Center provides a detailed report. Fix the
identified issue, rebuild the MSIX, and resubmit.

## Version Updates

For subsequent releases:

1. Increment the version in the tag (e.g. `v1.0.17`).
2. Rebuild and sign the MSIX with the new version (`1.0.17.0`).
3. In Partner Center, open the existing app → **Update** → upload the new package.
4. Update the Store listing description and release notes.
5. Submit.

The Microsoft Store requires each submission to have a higher version number than
the previously certified package.
