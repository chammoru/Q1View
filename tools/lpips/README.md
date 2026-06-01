# LPIPS ONNX export

One-time tooling that converts the reference [LPIPS](https://github.com/richzhang/PerceptualSimilarity)
perceptual-distance model into an ONNX graph that Q1View's Comparator loads via
ONNX Runtime. This is **not** part of the C++ build — run it once to produce the
model file, then commit/ship the resulting `.onnx`.

## Run

```powershell
cd tools\lpips
python -m venv .venv
.venv\Scripts\Activate.ps1
pip install -r requirements.txt

python export_lpips_onnx.py            # -> models\lpips_alex.onnx (tracked)
# python export_lpips_onnx.py --net squeeze
```

`torch` is a large download (~200 MB, CPU-only wheel is fine — no GPU needed).

## Output

`models\lpips_<net>.onnx` (default `lpips_alex.onnx`, ~5–15 MB for the `alex`
backbone). This is a **git-tracked** location (unlike `.deps\`, which is
gitignored): commit the generated `.onnx`. The planned LPIPS integration will have
the packaging scripts (`build/Package-Q1View.ps1`, `build/Package-Q1ViewMsix.ps1`)
copy `models\*.onnx` into the build output and MSIX package, with a post-build step
placing it next to `Comparator.exe` for local runs. (Not wired up yet.)

## Model contract (must match the C++ inference code)

| Aspect | Value |
|---|---|
| Inputs | `img0`, `img1` — both `float32`, NCHW, 3-channel **RGB** |
| Normalization | pixel values in **[-1, 1]** (`value / 127.5 - 1.0` for 8-bit) |
| Dynamic axes | batch, height, width are all dynamic (any resolution) |
| Min size | ≥ 64 px per side (AlexNet pooling stack) |
| Output | `dist` — shape `[N]`, one distance per pair; **lower = more similar**, 0 = identical |
| Opset | 12 |

If `onnxruntime` is installed, the script also verifies the exported graph
against the original torch model and reports the max absolute difference
(expected ~1e-5).

## Resolution handling (planned Comparator-side contract)

The ONNX graph is resolution-agnostic; the planned Comparator integration will
constrain what it feeds in:

- **Tiny frames (< 64 px on a side):** upscale/pad to the 64 px minimum before
  inference (below that the AlexNet pooling stack underflows).
- **Large frames (e.g. 4K):** two NCHW float32 4K tensors are ~190 MB of input
  alone, plus activations — too heavy per frame. Downsample (bilinear,
  aspect-preserving) so the longest side is ≤ a fixed cap (~1024 px) before
  inference. LPIPS is scale-sensitive, but the cap is applied uniformly across
  all frames so the per-frame curve stays comparable.

These policies will live in the C++ inference wrapper; the exported model never
changes.

## Backbone choice

`alex` (default) is small, fast, and the paper's recommended default for CPU
inference. `squeeze` is even smaller; `vgg` is more accurate but much larger and
slower — not recommended for per-frame video scanning.

## License note

LPIPS is BSD-2-Clause; the backbone weights come from torchvision's pretrained
models. Attribution must be added to `docs/STORE_LISTING.md` before shipping LPIPS support.
