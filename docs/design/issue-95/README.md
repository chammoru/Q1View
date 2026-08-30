# Q1View final icon family for issue #95

The approved **Bright Sky Q1** family is now the production identity for Q1View.

## Final review artifacts

- `q1view-icon-concept-j.png`: Viewer, Comparator, photo, video, and raw-file icons at their review sizes.
- `q1view-icon-windows-context-simulation.png`: native 16/24/32/48 px previews in taskbar, desktop fallback, and File Explorer contexts.

## Production source and outputs

`build/render_icons.py` is the single geometric source of truth. Running it writes:

- Viewer and Comparator multi-frame application ICOs.
- Photo, video, and raw-file multi-frame association ICOs.
- Microsoft Store/MSIX scale variants and unplated target-size assets.
- SVG inspection masters in `installer/msix/Assets`.

The release workflow runs the renderer before compiling the applications. MSIX packaging then runs `makepri.exe` and includes `resources.pri`, allowing Windows to resolve the DPI-qualified and unplated assets correctly.

```powershell
python build/render_icons.py
```

These are original Q1View geometric assets. No third-party artwork is included.
