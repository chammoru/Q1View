# Microsoft Store Listing — Q1View

Copy-paste ready text for the Microsoft Store Partner Center listing.

---

## App name

```
Q1View
```

---

## Short description (max 256 characters)

```
Pixel-level viewer and frame comparer for codec development and imaging QA. Inspect raw YUV and RGB buffers, step through video frames, and measure PSNR/SSIM differences between sources.
```

---

## Description (max 10,000 characters)

```
Q1View is a Windows toolkit for engineers who need to look at image and video data at the pixel level — not play it back casually.

It ships as two focused applications:

VIEWER
Open an image, raw frame dump, frame sequence, or video file. Zoom down to individual pixels, read exact component values (Y/U/V for raw YUV sources, R/G/B for decoded images), step through video frames one at a time, and inspect coordinates and selection regions. Video playback follows the source FPS or a user-selected rate and recovers from stalls without drifting.

Open multiple Viewer windows and enable Sync Input to keep navigation, zoom, pan, rotation, playback, and display settings synchronized across all of them — useful for side-by-side review of related sources without using Comparer.

COMPARER
Open two to four sources side by side. Comparer aligns them in a synchronized view and calculates PSNR and SSIM frame by frame. A bottom graph shows metric values across all frames; a side panel marks which frames contain differences. Use the PSNR or SSIM score to find the encoded output that differs most from the reference, then zoom in to see exactly which pixels changed.

SUPPORTED INPUTS
• BMP, JPEG, PNG, TIFF, WebP
• HEIF, HEIC, HIF (High Efficiency Image Format)
• AVIF / AV1 still images
• Raw YUV frame dumps (NV12, NV21, P010, I420, YUY2, and more) — resolution and color space read from the file name
• Video files supported by the bundled OpenCV/FFmpeg runtime
• Clipboard images (paste with Ctrl+V)
• Frame sequences — images in the same folder navigated with arrow keys or Page Up/Down

RAW FORMAT INSPECTION
When a raw YUV source is zoomed far enough to display pixel values, Viewer shows native Y, U, and V components sampled directly from the source planes — including subsampled and 10-bit formats — rather than values reconstructed from the displayed RGB image. Press V to switch between source YUV values and display RGB values.

WHO IS IT FOR
Q1View is designed for codec developers, imaging engineers, and video QA teams who need to verify encoder output, debug color conversion, review compression artifacts, or confirm that two processing pipelines produce identical results. It is not a general-purpose media player or photo manager.

TECHNICAL NOTES
• Windows x64 only
• Korean and Unicode file and folder names are supported
• No internet connection required; no data is collected or transmitted
• The Store app launches its dedicated Comparer workflow directly from Viewer
• Portable ZIP available for use without installation
```

---

## Feature list (up to 20 items, 200 characters each)

```
1. Zoom to individual pixels and read exact Y/U/V or R/G/B component values
2. Raw YUV format support: NV12, NV21, P010, I420, YUY2, and more
3. Side-by-side comparison of 2–4 sources with PSNR and SSIM measurement
4. Per-frame metric graph to quickly locate the worst-quality frames
5. Video playback aligned to source or user-selected FPS with stall recovery
6. Sync Input: link multiple Viewer windows for synchronized navigation
7. HEIF, HEIC, HIF, and AVIF still image support
8. Frame sequence navigation through same-folder images
9. Clipboard paste and screen capture support
10. Unicode and Korean path support
11. Launch the dedicated Comparer workflow directly from Viewer
12. Step through video frames one at a time with Left/Right arrow keys
13. Rotation, luma-only view, interpolation, and hex pixel value modes
14. Allow Different Resolution mode for cross-resolution comparisons
15. Portable ZIP available alongside the installer
```

---

## Search terms (up to 7 terms, 30 characters each)

```
1. YUV viewer
2. image comparer
3. PSNR SSIM
4. codec QA
5. raw frame inspector
6. HEIF viewer
7. pixel inspector
```

---

## Release notes template (for each update)

```
v1.0.XX

See the full changelog at:
https://github.com/chammoru/Q1View/blob/master/CHANGELOG.md
```

---

## Support information

| Field | Value |
| --- | --- |
| Support URL | https://github.com/chammoru/Q1View/issues |
| Website | https://github.com/chammoru/Q1View |
| Privacy Policy | https://github.com/chammoru/Q1View/blob/master/PRIVACY.md |
| Contact email | chammoru@gmail.com |

---

## Category

- **Primary**: Developer tools
- **Sub-category**: Utilities

---

## ESRB questionnaire guidance

Answer every content question **No**. Q1View:
- Has no user-generated content features
- Does not collect location data
- Has no in-app purchases or advertising
- Has no communication, chat, or social features
- Contains no violence, sexual content, or adult themes

Expected rating: **Everyone (E)** / **PEGI 3**
