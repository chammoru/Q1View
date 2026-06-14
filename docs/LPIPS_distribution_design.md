# LPIPS distribution design — Path B (runtime bundled, models downloaded)

> Addendum to issue #55. This revises **only** how the ONNX dependency is
> distributed and how the model is provisioned. The in-app metric/threading/UI
> design in the issue body is unchanged unless noted. Decided 2026-06-02.

## Summary of the change

The original issue bundles **both** the ONNX Runtime DLL and the LPIPS `.onnx`
model into the installer/MSIX. This addendum splits them:

- **`onnxruntime.dll` (the inference *runtime*) is bundled** with the app, kept
  **general-purpose** (not op-reduced). It is a *shared* runtime: every present
  and future ONNX model reuses the same DLL.
- **Models (`lpips_alex.onnx`, …) are downloaded on demand**, the first time the
  user selects an ML metric, verified by a pinned SHA-256, cached per-user, and
  loaded from the cache. No model ships in the installer.

This makes LPIPS the **first consumer of a reusable "ML add-on" capability**:
future ML features add only a small downloadable model, with **zero installer
growth** and no new native binary.

### Why this shape

- **Amortization** — the ~14 MB runtime is the real cost and it is paid **once**,
  shared across all future ML features. The model is small data.
- **Offline-by-default base** — the core app stays a fully offline inspection
  tool. Network access happens **only** on opt-in first use of an ML metric.
- **Store/MSIX-safe** — the sensitive, policy-restricted operation is
  *downloading and loading native code*. Here the native runtime is **bundled
  and signed inside the package**; only **data** (`.onnx`) is fetched at runtime,
  which is policy-clean.
- **Graceful degrade already exists** — `LpipsEngine` is already designed to drop
  to `available()==false` when the model/DLL is missing. "Offline / declined /
  hash mismatch" maps onto that existing path for free.

### Why NOT a minimal/op-reduced ORT build

An op-reduced build strips `onnxruntime.dll` down to only the operators the
LPIPS-alex graph uses (~14 MB → ~3–7 MB). That **defeats the shared-runtime
goal**: a future model needing any excluded operator would fail to load. Path B
keeps the **general** runtime. (If the project ever reverts to "LPIPS is a
one-off, fully bundled" then the minimal build becomes appropriate again — the
two are mutually exclusive.)

## fp16 model

The exported model is **float16-quantized** for a smaller download:

- `onnxconverter_common.float16.convert_float_to_float16(model, keep_io_types=True)`
  — internal weights become fp16; the graph **inputs/outputs stay fp32**, so the
  C++ caller's contract (feed fp32 NCHW RGB in [-1, 1]) is unchanged.
- **9.44 MB → 4.74 MB** (50%). Accuracy vs fp32: `max |Δdist| = 6.4e-06`
  (~0.02% relative) across 64²–720p random pairs; identical pairs still return
  exactly 0. Negligible for a perceptual metric.
- int8 dynamic quantization is **rejected**: 2.4 MB on disk but the ORT **CPU EP
  cannot execute it** (`ConvInteger` NOT_IMPLEMENTED for the conv-heavy graph).
  Static int8 would need a calibration set and risks accuracy drift — not worth
  it.
- On CPU, fp16 weights are up-cast to fp32 at compute time, so fp16 helps
  **download/disk size**, not speed (speed unchanged vs fp32). That is exactly
  the goal for an on-demand artifact.

Backbone default stays **alex** — the LPIPS paper's recommended *forward* metric
and the fastest; vgg is traditionally an optimization loss and is ~6× larger/slower
(a poor default for per-frame video scanning).

**Update (issue #75):** `vgg` is now also offered as an opt-in detailed mode
alongside `alex`, not a replacement. It is a second on-demand asset
(`lpips_vgg.onnx`, fp16, 29.6 MB, SHA-256
`5621992EE96D284567C147B7521097BE4219A213FB14FC2AA168F423FAE8FC2F`) in the same
`models-lpips-v1` release, provisioned through the same SHA-pinned download path.
Each backbone is a distinct metric with its own label and per-frame cache, since
LPIPS values from different backbones are not numerically interchangeable.
Regenerate either with `tools/lpips/export_lpips_onnx.py --net <alex|vgg> --fp16`.

## Measured size impact (portable ZIP, baseline 36.72 MB)

| | installer (ZIP) | delta |
|---|---|---|
| baseline | 36.72 MB | — |
| **Path B (runtime only bundled)** | **41.94 MB** | **+5.21 MB (+14%)** |
| *(ref) bundle everything, fp32 model* | 50.70 MB | +13.98 MB |
| *(ref) bundle everything, fp16 model* | 46.30 MB | +9.58 MB |

On-demand (first LPIPS use, **not** in installer): `lpips_alex.onnx` fp16 =
**4.74 MB**, SHA-256
`6742F1268D6E3D577A91ABC7B8D21AA34F2F94072809046656E1EDF968166006`.

Raw added to install tree: `onnxruntime.dll` 14.21 MB (ORT 1.26.0 CPU).
`onnxruntime_providers_shared.dll` (21 KB) is **not** shipped — only needed for
separate execution-provider DLLs, not the built-in CPU EP.

## Revised components

### Build-time runtime provisioning (mirrors the OpenCV pattern)

- **`build/Ensure-OnnxRuntimeDependency.ps1`** — downloads the official ORT CPU
  release zip into `.deps`, verifies SHA-256, extracts, writes a stamp. Modeled
  on `build/Ensure-OpenCvDependency.ps1` (mutex, cached-archive reuse, stamp
  check).
- **`OnnxRuntime.props`** (new, imported by `Comparator.vcxproj`) — modeled on
  `OpenCV4Release.props`:
  - `BeforeTargets="PrepareForBuild"` ensure-target invokes the script when the
    runtime is absent.
  - `ClCompile/AdditionalIncludeDirectories` += ORT `include` (only
    `onnxruntime_c_api.h` is needed; **no import lib** — dynamic C-API loading).
  - `AfterTargets="Build"` copies `onnxruntime.dll` to `$(OutDir)` so it sits
    next to `Comparator.exe` (and is picked up by packaging's `*.dll` glob).

### Packaging

- `build/Package-Q1View.ps1`, `installer/Q1View.iss`, `build/Package-Q1ViewMsix.ps1`
  ship **`onnxruntime.dll`** (already covered by the existing `*.dll` glob once it
  is in `$(OutDir)`).
- **Do not** copy `models/*.onnx` into dist/MSIX (reverses issue §6 for the
  model). The model is a runtime download.

### New: model provisioning module (Comparator) — the core of Path B

`Comparator/MlModelProvisioner.{h,cpp}` (or fold into `LpipsEngine`):

- **Manifest, compiled in** — a static table of required artifacts:
  `{ logical name, version, expected SHA-256, download URL }`. The URL is a
  version-pinned GitHub release asset
  (`…/releases/download/models-lpips-<ver>/lpips_alex.onnx`). The manifest is
  part of the signed app build → tamper-resistant pinning.
- **Cache** — `%LOCALAPPDATA%\Q1View\ml\<version>\lpips_alex.onnx`. Per-version
  subdir so an app update that bumps the model does not collide.
- **Provision flow** (first LPIPS selection):
  1. cache hit + SHA-256 ok → return path.
  2. miss → **opt-in prompt** ("LPIPS needs a one-time 4.7 MB download. Download
     now?"). On decline → unavailable (graceful degrade).
  3. download over **HTTPS** (WinHTTP) from the pinned URL to a temp file,
     **verify SHA-256**, atomic-move into cache. Any failure → unavailable; never
     load an unverified file.
- **Threading** — provisioning runs **off the UI thread** (reuse the LpipsScan
  worker, or a dedicated download thread); the UI shows progress / "downloading"
  and the existing not-computed label states.

### LpipsEngine

- Loads `onnxruntime.dll` by **absolute path from the exe directory** (bundled —
  unchanged from the issue design).
- Loads the model from the **provisioner's cache path** instead of next to the
  exe (the only change: model path source).

### Export tooling

- `tools/lpips/export_lpips_onnx.py` gains `--fp16` (default on for the shipped
  artifact) and prints the output SHA-256 to paste into the manifest.
- `tools/lpips/requirements.txt`: pin generation versions; record python/torch/
  lpips versions + artifact SHA-256 in the README.

### Store / MSIX

The MSIX bundles `onnxruntime.dll` (signed in-package). The model download is
**data only**, written to the app's LocalAppData cache — policy-clean. If Store
review still objects to any runtime download, the model can also be bundled in
the MSIX build specifically (the runtime/model split is per-distribution and the
engine does not care where the model comes from).

### Security model

- Native code (`onnxruntime.dll`) ships **inside the signed package** — never
  downloaded.
- Every downloaded artifact is HTTPS + **SHA-256 pinned** against the compiled
  manifest before use.
- Cache is per-user LocalAppData; a corrupt/tampered cache entry fails the hash
  check and is re-fetched, never loaded.

## Delta vs the issue's "Changed files"

- **+** `build/Ensure-OnnxRuntimeDependency.ps1`, `OnnxRuntime.props`
- **+** `Comparator/MlModelProvisioner.{h,cpp}` (download + SHA-256 + cache + opt-in UX)
- **~** `LpipsEngine`: model path comes from the provisioner cache, not `$(OutDir)`
- **~** `tools/lpips/export_lpips_onnx.py`: `--fp16` + SHA-256 print
- **~** Packaging (`Package-Q1View.ps1`, `Q1View.iss`, `Package-Q1ViewMsix.ps1`):
  bundle `onnxruntime.dll`; **do not** bundle `models/*.onnx`
- **=** everything else in the issue (metric metadata, lazy scan thread,
  single-writer invariant, generation/lifecycle, display logic) is unchanged

## Verification additions

- First LPIPS selection **offline** → opt-in prompt fails gracefully → metric
  disabled / "N/A", app stable.
- First selection **online**, accept → 4.7 MB download, SHA-256 verified, metric
  computes; second run uses the cache (no download).
- **Tampered cache** (flip a byte) → hash fails → re-download, never loads the
  bad file.
- Installer/MSIX contain `onnxruntime.dll` and **no** `.onnx`.
