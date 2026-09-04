#!/usr/bin/env python3
"""Render the final Q1View J icon family for every Windows surface.

This file is the canonical geometric source for the Viewer, Comparator, and
photo/video/raw file-association artwork. It writes the SVG review masters,
multi-frame ICO files, and every committed MSIX/Store PNG at its native size.

Requires Pillow. Run from the repository root:
    python build/render_icons.py
"""

from io import BytesIO
from pathlib import Path
import struct

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parent.parent
ASSETS = ROOT / "installer" / "msix" / "Assets"
ICO_SIZES = (16, 24, 32, 48, 64, 128, 256)
BRAND_TOP = "#2853C7"
BRAND_BOTTOM = "#713DAE"
SKY_TOP = "#28A9F0"
SKY_BOTTOM = "#6670DC"
SOFT_GOLD = "#F4C66D"
SUN = "#FFF1AE"


def _scaled_points(points, k):
    return [(int(x * k), int(y * k)) for x, y in points]


def render_icon(kind, size):
    """Render viewer/comparator/photo/video/raw directly at ``size`` pixels."""
    if kind not in {"viewer", "comparator", "photo", "video", "raw"}:
        raise ValueError(f"Unknown icon kind: {kind}")

    ss = max(2, min(8, 1024 // max(size, 1)))
    s = size * ss
    k = s / 256.0
    img = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)

    def rr(box, radius, fill, outline=None, width=1):
        d.rounded_rectangle(tuple(int(v * k) for v in box),
                            radius=max(1, int(radius * k)), fill=fill,
                            outline=outline, width=max(1, int(width * k)))

    def line(points, fill, width):
        d.line(_scaled_points(points, k), fill=fill,
               width=max(1, int(width * k)), joint="curve")

    def polygon(points, fill):
        d.polygon(_scaled_points(points, k), fill=fill)

    def ellipse(box, fill):
        d.ellipse(tuple(int(v * k) for v in box), fill=fill)

    def gradient_rr(box, radius, top, bottom):
        x0, y0, x1, y1 = (int(v * k) for v in box)
        width, height = max(1, x1 - x0), max(1, y1 - y0)
        ramp = Image.linear_gradient("L").resize((width, height))
        upper = Image.new("RGBA", (width, height), top)
        lower = Image.new("RGBA", (width, height), bottom)
        fill = Image.composite(lower, upper, ramp)
        mask = Image.new("L", (width, height), 0)
        ImageDraw.Draw(mask).rounded_rectangle(
            (0, 0, width - 1, height - 1),
            radius=max(1, int(radius * k)), fill=255)
        fill.putalpha(mask)
        img.alpha_composite(fill, (x0, y0))

    def gradient_polygon(points, top, bottom):
        mask = Image.new("L", (s, s), 0)
        ImageDraw.Draw(mask).polygon(_scaled_points(points, k), fill=255)
        ramp = Image.linear_gradient("L").resize((s, s))
        upper = Image.new("RGBA", (s, s), top)
        lower = Image.new("RGBA", (s, s), bottom)
        fill = Image.composite(lower, upper, ramp)
        fill.putalpha(mask)
        img.alpha_composite(fill)

    def q1_layer(small=False):
        mask = Image.new("L", (s, s), 0)
        md = ImageDraw.Draw(mask)
        stroke = int((30 if small else 23) * k)
        md.rounded_rectangle(tuple(int(v * k) for v in (3, 6, 207, 220)),
                             radius=int(49 * k), outline=255,
                             width=max(1, stroke))
        md.line(_scaled_points([(171, 184), (199, 212)], k), fill=255,
                width=max(1, stroke), joint="curve")
        if small:
            one = [(202, 43), (221, 22), (249, 22), (249, 214),
                   (221, 214), (221, 53), (202, 68)]
        else:
            one = [(207, 43), (226, 22), (249, 22), (249, 214),
                   (226, 214), (226, 53), (207, 68)]
        md.polygon(_scaled_points(one, k), fill=255)
        ramp = Image.linear_gradient("L").resize((s, s))
        upper = Image.new("RGBA", (s, s), BRAND_TOP)
        lower = Image.new("RGBA", (s, s), BRAND_BOTTOM)
        fill = Image.composite(lower, upper, ramp)
        fill.putalpha(mask)
        return fill

    if kind in {"photo", "video", "raw"}:
        rr((34, 18, 222, 238), 28, "#F8FAFF", "#CBD5E1", 5)
        polygon([(164, 18), (222, 76), (164, 76)], "#DCE5F2")
        gradient_rr((50, 112, 206, 220), 20, "#32A9EF", "#6B70DE")

        if kind == "photo":
            ellipse((76, 132, 100, 156), SUN)
            gradient_polygon([(101, 196), (158, 151), (195, 196)],
                             "#EEE9FF", "#B8B4F4")
            gradient_polygon([(65, 196), (110, 149), (151, 196)],
                             "#ECFFFA", "#B9EDEA")
        elif kind == "video":
            ellipse((86, 124, 170, 208), "#FFFFFF")
            polygon([(115, 142), (115, 190), (157, 166)], "#4050B8")
        else:
            for yy in range(2):
                for xx in range(2):
                    colour = "#FFFFFF" if (xx + yy) % 2 == 0 else "#9AF8E9"
                    rr((82 + xx * 48, 132 + yy * 39,
                        122 + xx * 48, 164 + yy * 39), 7, colour)

        mark = q1_layer(small=size <= 32)
        mark_size = max(1, int(88 * k))
        mark = mark.resize((mark_size, mark_size), Image.Resampling.LANCZOS)
        img.alpha_composite(mark, (int(82 * k), int(25 * k)))
        return img.resize((size, size), Image.Resampling.LANCZOS)

    gradient_rr((13, 17, 200, 211), 38, SKY_TOP, SKY_BOTTOM)
    img.alpha_composite(q1_layer(small=size <= 32))
    holes = (38, 94, 158) if size <= 32 else (32, 74, 120, 164)
    hole_w = 18 if size <= 32 else 16
    for x in holes:
        rr((x, 27, x + hole_w, 41), 3, "#E5FCFF")
        rr((x, 185, x + hole_w, 199), 3, "#E6DCFF")

    if kind == "comparator":
        gradient_polygon([(22, 184), (66, 118), (104, 184)],
                         "#ECFFFA", "#B7ECE8")
        gradient_polygon([(110, 184), (154, 118), (192, 184)],
                         "#EEE9FF", "#B5B2F2")
        line([(107, 51), (107, 188)], SOFT_GOLD, 10)
    else:
        gradient_polygon([(72, 184), (137, 110), (192, 184)],
                         "#EEE9FF", "#B5B2F2")
        gradient_polygon([(22, 184), (79, 116), (137, 184)],
                         "#ECFFFA", "#B7ECE8")
        ellipse((38, 56, 66, 84), SUN)
        ellipse((141, 127, 189, 175), "#4050B8")
        polygon([(157, 138), (157, 164), (179, 151)], "#FFFFFF")

    # Final approved optical balance: a modest 7.6% vertical expansion. The Q
    # body remains about 1.15 times taller than wide without the distorted 1.25
    # ratio of the discarded near-edge experiment.
    top = int(-5 * k)
    bottom = int(233 * k)
    img = img.crop((0, top, s, bottom)).resize((s, s), Image.Resampling.LANCZOS)
    return img.resize((size, size), Image.Resampling.LANCZOS)


def png_bytes(image):
    stream = BytesIO()
    image.save(stream, "PNG")
    return stream.getvalue()


def write_ico(kind, path):
    frames = [(size, png_bytes(render_icon(kind, size))) for size in ICO_SIZES]
    header = struct.pack("<HHH", 0, 1, len(frames))
    offset = 6 + 16 * len(frames)
    body = bytearray()
    for size, data in frames:
        width_byte = 0 if size >= 256 else size
        header += struct.pack("<BBBBHHII", width_byte, width_byte, 0, 0,
                              1, 32, len(data), offset)
        body.extend(data)
        offset += len(data)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(header + body)
    print("wrote", path.relative_to(ROOT))


def write_msix_assets():
    square = {
        "StoreLogo": [(50, ""), (63, ".scale-125"), (75, ".scale-150"),
                      (100, ".scale-200"), (200, ".scale-400")],
        "Square44x44Logo": [(44, ""), (55, ".scale-125"), (66, ".scale-150"),
                            (88, ".scale-200"), (176, ".scale-400")],
        "Square150x150Logo": [(150, ""), (188, ".scale-125"),
                              (225, ".scale-150"), (300, ".scale-200"),
                              (600, ".scale-400")],
    }
    for base, variants in square.items():
        for pixels, suffix in variants:
            render_icon("viewer", pixels).save(ASSETS / f"{base}{suffix}.png")

    for width, height, suffix in ((310, 150, ""), (388, 188, ".scale-125"),
                                  (465, 225, ".scale-150"),
                                  (620, 300, ".scale-200"),
                                  (1240, 600, ".scale-400")):
        tile = render_icon("viewer", height)
        canvas = Image.new("RGBA", (width, height), (0, 0, 0, 0))
        canvas.alpha_composite(tile, ((width - height) // 2, 0))
        canvas.save(ASSETS / f"Wide310x150Logo{suffix}.png")

    for pixels in (16, 24, 32, 48, 256):
        icon = render_icon("viewer", pixels)
        for suffix in ("", "_altform-unplated", "_altform-lightunplated"):
            icon.save(ASSETS / f"Square44x44Logo.targetsize-{pixels}{suffix}.png")

    for kind, base in (("photo", "FilePhotoLogo"),
                       ("video", "FileVideoLogo"),
                       ("raw", "FileRawLogo")):
        for pixels, suffix in ((44, ""), (55, ".scale-125"),
                               (66, ".scale-150"), (88, ".scale-200"),
                               (176, ".scale-400")):
            render_icon(kind, pixels).save(ASSETS / f"{base}{suffix}.png")
    print("wrote installer/msix/Assets/*.png")


def _svg_header(kind):
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="256" height="256" viewBox="0 0 256 256">
  <!-- Generated by build/render_icons.py from the approved Q1View J geometry ({kind}). -->
  <defs>
    <linearGradient id="brand" x1="0" y1="0" x2="0" y2="1"><stop stop-color="{BRAND_TOP}"/><stop offset="1" stop-color="{BRAND_BOTTOM}"/></linearGradient>
    <linearGradient id="sky" x1="0" y1="0" x2="0" y2="1"><stop stop-color="{SKY_TOP}"/><stop offset="1" stop-color="{SKY_BOTTOM}"/></linearGradient>
    <linearGradient id="mint" x1="0" y1="0" x2="0" y2="1"><stop stop-color="#ECFFFA"/><stop offset="1" stop-color="#B7ECE8"/></linearGradient>
    <linearGradient id="lavender" x1="0" y1="0" x2="0" y2="1"><stop stop-color="#EEE9FF"/><stop offset="1" stop-color="#B5B2F2"/></linearGradient>
  </defs>'''


def _q1_svg(indent="    "):
    return f'''{indent}<rect x="3" y="6" width="204" height="214" rx="49" fill="none" stroke="url(#brand)" stroke-width="23"/>
{indent}<line x1="171" y1="184" x2="199" y2="212" stroke="url(#brand)" stroke-width="23" stroke-linecap="round"/>
{indent}<polygon points="207,43 226,22 249,22 249,214 226,214 226,53 207,68" fill="url(#brand)"/>'''


def app_svg(kind):
    comparator = kind == "comparator"
    content = [_svg_header(kind),
               '  <g transform="matrix(1 0 0 1.075630 0 5.378151)">',
               '    <rect x="13" y="17" width="187" height="194" rx="38" fill="url(#sky)"/>',
               _q1_svg()]
    for x in (32, 74, 120, 164):
        content.append(f'    <rect x="{x}" y="27" width="16" height="14" rx="3" fill="#E5FCFF"/>')
        content.append(f'    <rect x="{x}" y="185" width="16" height="14" rx="3" fill="#E6DCFF"/>')
    if comparator:
        content += ['    <polygon points="22,184 66,118 104,184" fill="url(#mint)"/>',
                    '    <polygon points="110,184 154,118 192,184" fill="url(#lavender)"/>',
                    f'    <line x1="107" y1="51" x2="107" y2="188" stroke="{SOFT_GOLD}" stroke-width="10"/>']
    else:
        content += ['    <polygon points="72,184 137,110 192,184" fill="url(#lavender)"/>',
                    '    <polygon points="22,184 79,116 137,184" fill="url(#mint)"/>',
                    f'    <circle cx="52" cy="70" r="14" fill="{SUN}"/>',
                    '    <circle cx="165" cy="151" r="24" fill="#4050B8"/>',
                    '    <polygon points="157,138 157,164 179,151" fill="#FFFFFF"/>']
    content += ['  </g>', '</svg>', '']
    return "\n".join(content)


def file_svg(kind):
    content = [_svg_header(kind),
               '  <rect x="34" y="18" width="188" height="220" rx="28" fill="#F8FAFF" stroke="#CBD5E1" stroke-width="5"/>',
               '  <polygon points="164,18 222,76 164,76" fill="#DCE5F2"/>',
               '  <rect x="50" y="112" width="156" height="108" rx="20" fill="url(#sky)"/>']
    if kind == "photo":
        content += [f'  <circle cx="88" cy="144" r="12" fill="{SUN}"/>',
                    '  <polygon points="101,196 158,151 195,196" fill="url(#lavender)"/>',
                    '  <polygon points="65,196 110,149 151,196" fill="url(#mint)"/>']
    elif kind == "video":
        content += ['  <circle cx="128" cy="166" r="42" fill="#FFFFFF"/>',
                    '  <polygon points="115,142 115,190 157,166" fill="#4050B8"/>']
    else:
        for yy in range(2):
            for xx in range(2):
                colour = "#FFFFFF" if (xx + yy) % 2 == 0 else "#9AF8E9"
                content.append(f'  <rect x="{82 + xx * 48}" y="{132 + yy * 39}" width="40" height="32" rx="7" fill="{colour}"/>')
    content += ['  <g transform="translate(82 25) scale(.34375)">', _q1_svg('    '), '  </g>', '</svg>', '']
    return "\n".join(content)


def write_svg_masters():
    for name, body in (("Q1View.svg", app_svg("viewer")),
                       ("Comparator.svg", app_svg("comparator")),
                       ("FilePhoto.svg", file_svg("photo")),
                       ("FileVideo.svg", file_svg("video")),
                       ("FileRaw.svg", file_svg("raw"))):
        (ASSETS / name).write_text(body, encoding="utf-8")
        print("wrote", (ASSETS / name).relative_to(ROOT))


def verify_outputs():
    expected_sizes = {(size, size) for size in ICO_SIZES}
    ico_paths = (ROOT / "Viewer" / "res" / "Viewer.ico",
                 ROOT / "ViewerQt" / "Viewer.ico",
                 ROOT / "Comparator" / "res" / "Comparator.ico",
                 ROOT / "Viewer" / "res" / "Q1ViewPhoto.ico",
                 ROOT / "Viewer" / "res" / "Q1ViewVideo.ico",
                 ROOT / "Viewer" / "res" / "Q1ViewRaw.ico")
    for path in ico_paths:
        with Image.open(path) as icon:
            sizes = set(icon.ico.sizes())
        if sizes != expected_sizes:
            raise RuntimeError(f"{path}: ICO sizes {sizes}, expected {expected_sizes}")

    for kind in ("viewer", "comparator", "photo", "video", "raw"):
        for size in ICO_SIZES:
            if render_icon(kind, size).getchannel("A").getbbox() is None:
                raise RuntimeError(f"{kind} {size}px rendered empty")

    required = ("Square44x44Logo.png", "Square150x150Logo.png",
                "Wide310x150Logo.png", "StoreLogo.png", "FilePhotoLogo.png",
                "FileVideoLogo.png", "FileRawLogo.png",
                "Square44x44Logo.targetsize-24_altform-unplated.png")
    missing = [name for name in required if not (ASSETS / name).is_file()]
    if missing:
        raise RuntimeError(f"Missing generated assets: {', '.join(missing)}")
    print("verified all ICO frames and required MSIX assets")


if __name__ == "__main__":
    ASSETS.mkdir(parents=True, exist_ok=True)
    write_svg_masters()
    write_ico("viewer", ROOT / "Viewer" / "res" / "Viewer.ico")
    write_ico("viewer", ROOT / "ViewerQt" / "Viewer.ico")
    write_ico("comparator", ROOT / "Comparator" / "res" / "Comparator.ico")
    write_ico("photo", ROOT / "Viewer" / "res" / "Q1ViewPhoto.ico")
    write_ico("video", ROOT / "Viewer" / "res" / "Q1ViewVideo.ico")
    write_ico("raw", ROOT / "Viewer" / "res" / "Q1ViewRaw.ico")
    write_msix_assets()
    verify_outputs()
    print("done")
