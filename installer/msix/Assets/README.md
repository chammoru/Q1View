# Q1View MSIX Assets

Place the required PNG logo files in this directory before running
`Package-Q1ViewMsix.ps1`. The script validates their presence at startup.

## Source of truth

`Q1View.svg` is the vector master for every logo in this folder. All the PNGs
below are rendered from it, so edit the SVG and re-render rather than touching
the PNGs by hand. To regenerate at the correct size with ImageMagick, e.g.:

```powershell
# density 576 = 256 (SVG viewBox) * 576 / 72 -> a 2048px master to downscale from
magick -background none -density 576 Q1View.svg master.png
magick master.png -filter Lanczos -resize 150x150 -background none Square150x150Logo.png
# wide tiles center the square icon on a transparent canvas:
magick master.png -filter Lanczos -resize 150x150 -background none -gravity center -extent 310x150 Wide310x150Logo.png
```

## Required files (scale-100 minimum)

| File | Size | Purpose |
| --- | --- | --- |
| `Square44x44Logo.png` | 44 × 44 px | Taskbar icon and Start menu small tile |
| `Square150x150Logo.png` | 150 × 150 px | Start menu medium tile |
| `Wide310x150Logo.png` | 310 × 150 px | Start menu wide tile |
| `StoreLogo.png` | 50 × 50 px | Microsoft Store listing thumbnail |

## Additional scale variants for Store submission

The Store requires assets at multiple DPI scales. Name them with the scale suffix:

| Scale | Multiplier | Square44 | Square150 | Wide310x150 | StoreLogo |
| --- | --- | --- | --- | --- | --- |
| 100% | 1× | 44 × 44 | 150 × 150 | 310 × 150 | 50 × 50 |
| 125% | 1.25× | 55 × 55 | 188 × 188 | 388 × 188 | 63 × 63 |
| 150% | 1.5× | 66 × 66 | 225 × 225 | 465 × 225 | 75 × 75 |
| 200% | 2× | 88 × 88 | 300 × 300 | 620 × 300 | 100 × 100 |
| 400% | 4× | 176 × 176 | 600 × 600 | 1240 × 600 | 200 × 200 |

Example naming: `Square44x44Logo.scale-100.png`, `Square44x44Logo.scale-200.png`.

When scaled variants are present, Windows picks the best fit automatically.
The bare filename (`Square44x44Logo.png`) serves as the scale-100 fallback.

## Design guidelines

- Use a transparent background.
- Keep important content away from the corners; Windows clips them with rounded
  corners.
- Store assets must not include screenshot content or promotional text in the
  logo files themselves.
- Follow the Microsoft Store visual asset requirements:
  https://learn.microsoft.com/en-us/windows/apps/design/style/iconography/app-icon-design
