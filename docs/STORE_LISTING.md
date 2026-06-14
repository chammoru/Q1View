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
Pixel-level viewer and frame comparer for codec, imaging, and QA work. Inspect raw YUV/RGB buffers, step through video frames, compare 2-4 sources, and measure PSNR, SSIM, and LPIPS differences.
```

---

## Description (max 10,000 characters)

```
Q1View is a Windows toolkit for engineers who need to inspect image and video data at the pixel level instead of just playing it back.

It is built for codec developers, imaging engineers, computer vision engineers, researchers, and QA teams who need to verify raw buffers, decoded frames, compression artifacts, color conversion, timing behavior, and small visual differences between processing pipelines.

Q1View includes two focused applications:

VIEWER

Open an image, raw frame dump, frame sequence, or video file. Zoom down to individual pixels, inspect exact coordinates, read component values, step through frames one at a time, and capture the current view or selected region.

For raw YUV sources, Viewer can show native Y, U, and V values sampled directly from the source planes, including subsampled and 10-bit formats, instead of only showing values reconstructed from the displayed RGB image. Press V to switch between source YUV values and display RGB values.

Raw input can be reinterpreted quickly without reopening the file. Press N to cycle color spaces and D to cycle preset resolutions. This is useful when debugging raw YUV/RGB dumps whose format, size, or color interpretation must be checked quickly.

Viewer also supports image-sequence navigation, video playback, frame/time progress display, clipboard paste, screen capture, and a thumbnail browser for browsing the current folder. Open multiple Viewer windows and enable Sync Input to keep navigation, zoom, pan, rotation, playback, and display settings synchronized across them.

COMPARATOR

Open two to four sources side by side. Comparator aligns them in a synchronized view and calculates objective quality metrics frame by frame, including PSNR, SSIM, and LPIPS. A bottom graph shows metric values across frames, the timeline helps locate frames that contain differences, and synchronized region selection can show crop scores next to whole-image scores.

Use Comparator to compare a reference image or video frame against processed outputs, encoded results, or alternative pipelines. Zoom in to inspect the exact pixels that changed, use the visual difference overlay to find small mismatches, and jump through frame sequences or video frames with synchronized controls.

SUPPORTED INPUTS

• BMP, JPEG, PNG, TIFF, WebP
• HEIF, HEIC, HIF
• AVIF / AV1 still images
• Raw YUV and RGB frame dumps, including NV12, NV21, P010, I420, YUY2, and more
• Video files supported by the bundled OpenCV/FFmpeg runtime
• Clipboard images, including screenshots pasted with Ctrl+V
• Frame sequences, including images in the same folder navigated with arrow keys or Page Up/Page Down

TYPICAL USE CASES

• Validate encoder or decoder output against a reference
• Inspect raw YUV/RGB buffers and confirm color-space interpretation
• Compare camera pipeline, ISP, or image-processing outputs
• Find the frame with the worst PSNR, SSIM, or LPIPS score
• Check whether two processing pipelines produce identical pixels
• Debug compression artifacts, off-by-one shifts, timing issues, or color conversion problems
• Capture exact pixel values and screenshots for bug reports or technical analysis

TECHNICAL NOTES

• Windows x64 only
• Korean and Unicode file and folder names are supported
• Core viewing and comparison workflows work offline
• Q1View does not collect or transmit user image or video data
• The Microsoft Store version launches the dedicated Comparator workflow directly from Viewer
• Microsoft Store builds can check for available Store updates in the background
• A portable ZIP package is also available from the GitHub Releases page
• This app requires the Microsoft Visual C++ 2015-2022 Redistributable (x64) runtime. The Microsoft Store installs it automatically alongside Q1View.
```

---

## Feature list (up to 20 items, 200 characters each)

```
1. Zoom to individual pixels and read exact Y/U/V or R/G/B component values
2. Raw YUV format support: NV12, NV21, P010, I420, YUY2, and more
3. Side-by-side comparison of 2–4 sources with PSNR, SSIM, and LPIPS measurement
4. Per-frame metric graph to quickly locate the worst-quality frames
5. Synchronized Comparator region selection with crop PSNR, SSIM, and LPIPS readouts
6. Video playback aligned to source or user-selected FPS with frame/time progress and stall recovery
7. Sync Input: link multiple Viewer windows for synchronized navigation
8. HEIF, HEIC, HIF, and AVIF still image support
9. Frame sequence navigation through same-folder images
10. Clipboard paste and screen capture support
11. Unicode and Korean path support
12. Launch the dedicated Comparator workflow directly from Viewer
13. Step through video frames one at a time with Left/Right arrow keys
14. Rotation, luma-only view, interpolation, and hex pixel value modes
15. Allow Different Resolution mode for cross-resolution comparisons
16. Portable ZIP available alongside the installer
17. Cycle raw color space (N) and preset resolutions (D) without reopening the file
```

---

## Search terms (up to 7 terms, 30 characters each)

```
1. YUV viewer
2. image comparer
3. PSNR SSIM LPIPS
4. codec QA
5. raw frame inspector
6. HEIF viewer
7. pixel inspector
```

---

## Release notes template (for each update)

```
vX.Y.Z

See the full changelog at:
https://github.com/chammoru/Q1View/blob/master/CHANGELOG.md
```

Release CI fills in the actual "What's New" automatically from the matching
`CHANGELOG.md` section, so this template is only a manual fallback.

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
