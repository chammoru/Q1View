#!/usr/bin/env python3
"""Export a LPIPS perceptual-distance model to ONNX for Q1View.

LPIPS (Zhang et al., "The Unreasonable Effectiveness of Deep Features as a
Perceptual Metric", CVPR 2018) compares two images through the features of a
pretrained CNN backbone (AlexNet / SqueezeNet / VGG) and a small learned linear
head. This script wraps the reference `lpips` package into a two-input ONNX
graph that Q1View's Comparator loads via ONNX Runtime.

Input/output contract baked into the exported graph (the C++ side must match):
  - Two inputs `img0`, `img1`, both NCHW, 3-channel RGB, dtype float32.
  - Pixel values normalized to [-1, 1] (i.e. value/127.5 - 1.0 for 8-bit RGB).
    This matches `lpips.LPIPS.forward(..., normalize=False)`, the default.
  - Output `dist`: shape [N], one perceptual distance per image pair. Lower is
    more similar; 0 means identical. Typical range ~[0, ~1] but not bounded.
  - Height/width/batch are dynamic axes, so any resolution works. The backbones
    need a minimum spatial size (>= 64 px is safe for AlexNet).

Usage:
  pip install -r requirements.txt
  python export_lpips_onnx.py                 # -> <repo>/models/lpips_alex.onnx (runtime download asset)
  python export_lpips_onnx.py --net squeeze
  python export_lpips_onnx.py --out C:/path/lpips.onnx --opset 12

Verification: if `onnxruntime` is importable, the script runs the exported graph
and the original torch model on the same random input and reports the max abs
difference (should be ~1e-5 or smaller).
"""

import argparse
import hashlib
import os
import sys


def repo_root():
    # tools/lpips/export_lpips_onnx.py -> repo root is two levels up.
    here = os.path.dirname(os.path.abspath(__file__))
    return os.path.normpath(os.path.join(here, os.pardir, os.pardir))


def default_out(net):
    # Output under <repo>/models/. This is NOT committed (models/*.onnx is
    # gitignored): the model is a runtime download / GitHub release asset
    # (Path B, docs/LPIPS_distribution_design.md). The local copy is the
    # dev-path fallback MlModelProvisioner reads. Paste the printed SHA256
    # into the provisioning manifest and upload the file as the release asset.
    return os.path.join(repo_root(), "models", "lpips_%s.onnx" % net)


def build_model(net):
    import torch
    import lpips

    class LpipsWrapper(torch.nn.Module):
        """Adapts lpips.LPIPS to a clean two-in / one-out (flat) signature.

        lpips returns shape [N, 1, 1, 1]; we flatten to [N] so the ONNX output
        is a simple per-pair vector that the C++ caller can read directly.
        """

        def __init__(self, backbone):
            super().__init__()
            # spatial=False -> single averaged distance per pair (not a map).
            # The linear head ("lin") weights are loaded from the pretrained
            # checkpoint that ships with the lpips package.
            self.lpips = lpips.LPIPS(net=backbone, spatial=False)

        def forward(self, img0, img1):
            # normalize=False: inputs are already in [-1, 1].
            d = self.lpips(img0, img1, normalize=False)
            return d.reshape(-1)

    model = LpipsWrapper(net)
    model.eval()
    return model


def export(net, out_path, opset):
    import torch

    model = build_model(net)

    os.makedirs(os.path.dirname(out_path), exist_ok=True)

    # 64x64 dummies: large enough for every backbone's pooling stack while the
    # dynamic axes below let the deployed graph accept any resolution.
    dummy0 = torch.rand(1, 3, 64, 64) * 2.0 - 1.0
    dummy1 = torch.rand(1, 3, 64, 64) * 2.0 - 1.0

    with torch.no_grad():
        torch.onnx.export(
            model,
            (dummy0, dummy1),
            out_path,
            input_names=["img0", "img1"],
            output_names=["dist"],
            opset_version=opset,
            do_constant_folding=True,
            dynamic_axes={
                "img0": {0: "batch", 2: "height", 3: "width"},
                "img1": {0: "batch", 2: "height", 3: "width"},
                "dist": {0: "batch"},
            },
        )

    print("Exported %s backbone -> %s" % (net, out_path))
    return model, dummy0, dummy1


def convert_to_fp16(out_path):
    """Quantize the exported graph to float16 in place.

    keep_io_types=True leaves the graph inputs/outputs as float32, so the C++
    caller's contract (feed fp32 NCHW RGB in [-1, 1], read fp32 distance) is
    unchanged; only the internal weights become fp16. This ~halves the file size
    (the model is a runtime download in Q1View). On the CPU EP fp16 weights are
    up-cast at compute time, so this is a size optimization, not a speed one.
    Accuracy impact is negligible for a perceptual distance (max |Δ| ~6e-06).
    """
    import onnx
    from onnxconverter_common import float16

    model = onnx.load(out_path)
    model16 = float16.convert_float_to_float16(model, keep_io_types=True)
    onnx.save(model16, out_path)
    print("Quantized to float16 (keep_io_types) -> %s" % out_path)


def consolidate_single_file(out_path):
    """Fold any external weight data back into a single self-contained .onnx.

    The torch>=2.x dynamo exporter offloads large initializers to a sibling
    `<model>.onnx.data` file. Q1View provisions each model as ONE downloadable,
    SHA-256-pinned artifact, so re-embed the weights and delete the side file.
    A no-op when the graph is already self-contained.
    """
    import onnx

    model = onnx.load(out_path)  # load_external_data=True by default
    onnx.save_model(model, out_path, save_as_external_data=False)
    side = out_path + ".data"
    if os.path.exists(side):
        os.remove(side)


def sha256_of(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def verify(out_path, model, dummy0, dummy1):
    try:
        import numpy as np
        import onnxruntime as ort
    except ImportError:
        print("onnxruntime/numpy not installed; skipping numerical verification.")
        return True

    import torch

    with torch.no_grad():
        ref = model(dummy0, dummy1).cpu().numpy().reshape(-1)

    sess = ort.InferenceSession(out_path, providers=["CPUExecutionProvider"])
    got = sess.run(
        ["dist"],
        {"img0": dummy0.numpy(), "img1": dummy1.numpy()},
    )[0].reshape(-1)

    max_abs = float(np.max(np.abs(ref - got)))
    print("torch vs onnxruntime max |diff| = %.3e (torch=%.6f, ort=%.6f)"
          % (max_abs, float(ref[0]), float(got[0])))

    # Also sanity-check a different resolution to confirm dynamic axes work.
    big0 = (torch.rand(1, 3, 128, 200) * 2.0 - 1.0).numpy()
    big1 = (torch.rand(1, 3, 128, 200) * 2.0 - 1.0).numpy()
    sess.run(["dist"], {"img0": big0, "img1": big1})
    print("Dynamic-shape run (128x200) OK.")

    ok = max_abs < 1e-3
    if not ok:
        print("WARNING: difference larger than expected (1e-3).", file=sys.stderr)
    return ok


def main(argv=None):
    parser = argparse.ArgumentParser(description="Export LPIPS to ONNX for Q1View.")
    parser.add_argument("--net", default="alex", choices=["alex", "squeeze", "vgg"],
                        help="backbone (default: alex; small & fast, recommended)")
    parser.add_argument("--out", default=None,
                        help="output .onnx path (default: <repo>/models/lpips_<net>.onnx, tracked)")
    parser.add_argument("--opset", type=int, default=12,
                        help="ONNX opset version (default: 12)")
    parser.add_argument("--fp16", action="store_true",
                        help="float16-quantize weights (halves size; recommended for the "
                             "shipped/downloadable artifact). I/O stay float32.")
    args = parser.parse_args(argv)

    out_path = args.out or default_out(args.net)

    try:
        model, d0, d1 = export(args.net, out_path, args.opset)
        if args.fp16:
            convert_to_fp16(out_path)
        # Guarantee one self-contained file (newer torch exporters split weights
        # into a sidecar .onnx.data; Q1View ships a single pinned artifact).
        consolidate_single_file(out_path)
    except ImportError as e:
        print("Missing dependency: %s\nRun: pip install -r requirements.txt" % e,
              file=sys.stderr)
        return 2

    # verify() runs the *final* artifact (fp16 if converted) against torch.
    ok = verify(out_path, model, d0, d1)
    print("SHA256(%s) = %s" % (os.path.basename(out_path), sha256_of(out_path)))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
