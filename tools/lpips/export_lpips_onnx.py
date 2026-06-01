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
  python export_lpips_onnx.py                 # -> <repo>/models/lpips_alex.onnx (tracked)
  python export_lpips_onnx.py --net squeeze
  python export_lpips_onnx.py --out C:/path/lpips.onnx --opset 12

Verification: if `onnxruntime` is importable, the script runs the exported graph
and the original torch model on the same random input and reports the max abs
difference (should be ~1e-5 or smaller).
"""

import argparse
import os
import sys


def repo_root():
    # tools/lpips/export_lpips_onnx.py -> repo root is two levels up.
    here = os.path.dirname(os.path.abspath(__file__))
    return os.path.normpath(os.path.join(here, os.pardir, os.pardir))


def default_out(net):
    # NOTE: tracked location (NOT .deps, which is gitignored) so the generated
    # model can be committed and picked up by the packaging scripts.
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
    args = parser.parse_args(argv)

    out_path = args.out or default_out(args.net)

    try:
        model, d0, d1 = export(args.net, out_path, args.opset)
    except ImportError as e:
        print("Missing dependency: %s\nRun: pip install -r requirements.txt" % e,
              file=sys.stderr)
        return 2

    ok = verify(out_path, model, d0, d1)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
