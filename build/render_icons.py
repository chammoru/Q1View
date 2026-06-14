#!/usr/bin/env python3
"""Render the Q1View app icons from their SVG masters, fresh at each target size.

The SVG masters (installer/msix/Assets/Q1View.svg for the Viewer, Comparator.svg
for the Comparator) are the single source of truth. Each output size is rasterised
at its own resolution (per-size supersample + one Lanczos downscale) with clean
straight alpha -- it is *not* a downscale of one big bitmap, so small sizes stay
crisp and transparent edges stay clean.

Outputs:
  - Viewer/res/Viewer.ico, ViewerQt/Viewer.ico   (from Q1View.svg)
  - Comparator/res/Comparator.ico                (from Comparator.svg)
  - installer/msix/Assets/*.png  (Store/MSIX logos, from Q1View.svg)

Requires Pillow (pip install pillow). Run from the repo root: python build/render_icons.py
"""
import os
import re
import struct
import xml.etree.ElementTree as ET

from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VIEWER_SVG = os.path.join(ROOT, "installer", "msix", "Assets", "Q1View.svg")
COMPARATOR_SVG = os.path.join(ROOT, "installer", "msix", "Assets", "Comparator.svg")

ICO_SIZES = [16, 24, 32, 48, 64, 128, 256]


def _color(value):
    """#RRGGBB -> (r, g, b, 255)."""
    v = value.strip().lstrip("#")
    return (int(v[0:2], 16), int(v[2:4], 16), int(v[4:6], 16), 255)


def parse_svg(path):
    """Parse the simple rect/circle master into (viewbox, shapes)."""
    tree = ET.parse(path)
    root = tree.getroot()
    vb = root.get("viewBox", "0 0 256 256").split()
    view = float(vb[2])
    shapes = []
    for el in root.iter():
        tag = el.tag.split("}")[-1]
        if tag == "rect":
            shapes.append(("rr", float(el.get("x")), float(el.get("y")),
                           float(el.get("width")), float(el.get("height")),
                           float(el.get("rx", 0)), _color(el.get("fill"))))
        elif tag == "circle":
            shapes.append(("circ", float(el.get("cx")), float(el.get("cy")),
                           float(el.get("r")), _color(el.get("fill"))))
    return view, shapes


def render(view, shapes, size):
    ss = max(1, min(8, 2048 // size))   # more AA for small icons, capped for large
    s = size * ss
    k = s / view
    img = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    for shape in shapes:
        if shape[0] == "rr":
            _, x, y, w, h, r, fill = shape
            d.rounded_rectangle([x * k, y * k, (x + w) * k, (y + h) * k],
                                radius=r * k, fill=fill)
        else:
            _, cx, cy, rad, fill = shape
            d.ellipse([(cx - rad) * k, (cy - rad) * k, (cx + rad) * k, (cy + rad) * k],
                      fill=fill)
    return img.resize((size, size), Image.LANCZOS) if ss != 1 else img


def write_ico(frames, path):
    """frames: list of (size, PNG bytes). Stores each as a PNG-compressed frame."""
    n = len(frames)
    out = struct.pack("<HHH", 0, 1, n)
    offset = 6 + 16 * n
    body = b""
    for size, png in frames:
        wb = 0 if size >= 256 else size
        out += struct.pack("<BBBBHHII", wb, wb, 0, 0, 1, 32, len(png), offset)
        body += png
        offset += len(png)
    with open(path, "wb") as f:
        f.write(out + body)


def png_bytes(img):
    import io
    buf = io.BytesIO()
    img.save(buf, "PNG")
    return buf.getvalue()


def build_app_icon(svg, out_paths):
    view, shapes = parse_svg(svg)
    frames = [(s, png_bytes(render(view, shapes, s))) for s in ICO_SIZES]
    for p in out_paths:
        write_ico(frames, os.path.join(ROOT, p))
        print("wrote", p)


def build_msix():
    view, shapes = parse_svg(VIEWER_SVG)
    assets = os.path.join(ROOT, "installer", "msix", "Assets")
    square = {
        "StoreLogo": [(50, ""), (63, ".scale-125"), (75, ".scale-150"), (100, ".scale-200"), (200, ".scale-400")],
        "Square44x44Logo": [(44, ""), (55, ".scale-125"), (66, ".scale-150"), (88, ".scale-200"), (176, ".scale-400")],
        "Square150x150Logo": [(150, ""), (188, ".scale-125"), (225, ".scale-150"), (300, ".scale-200"), (600, ".scale-400")],
    }
    for base, variants in square.items():
        for px, suffix in variants:
            render(view, shapes, px).save(os.path.join(assets, f"{base}{suffix}.png"))
    # Wide tiles centre the square icon (rendered at the canvas height) on a
    # transparent canvas.
    wide = [(310, 150, ""), (388, 188, ".scale-125"), (465, 225, ".scale-150"),
            (620, 300, ".scale-200"), (1240, 600, ".scale-400")]
    for w, h, suffix in wide:
        tile = render(view, shapes, h)
        canvas = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        canvas.paste(tile, ((w - h) // 2, 0), tile)
        canvas.save(os.path.join(assets, f"Wide310x150Logo{suffix}.png"))
    # Target-size + unplated variants for the taskbar and the Start "all apps"
    # list. Without the *_altform-unplated assets Windows draws the icon on a
    # solid square "plate", so the transparent rounded-corner artwork shows a
    # square edge on the taskbar. Same artwork serves plated/unplated/light.
    for px in (16, 24, 32, 48, 256):
        icon = render(view, shapes, px)
        for suffix in ("", "_altform-unplated", "_altform-lightunplated"):
            icon.save(os.path.join(assets, f"Square44x44Logo.targetsize-{px}{suffix}.png"))
    print("wrote installer/msix/Assets/*.png")


if __name__ == "__main__":
    build_app_icon(VIEWER_SVG, ["Viewer/res/Viewer.ico", "ViewerQt/Viewer.ico"])
    build_app_icon(COMPARATOR_SVG, ["Comparator/res/Comparator.ico"])
    build_msix()
    print("done")
