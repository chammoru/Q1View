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

Viewer and Comparator use coordinated Pretendard typography for app-drawn Korean and English text, window titles, application menus and app-owned context menus. Text follows Windows text-size and display scaling settings. Viewer keeps resolution, pixel-format and FPS menus directly accessible and places Browse immediately left of Options, without a duplicate Thumbnail Browser entry in File. Options groups image scaling, video autoplay and theme settings without separate View or More menus. Narrow windows wrap the menus; open files through File > Open or Ctrl+O without a duplicate Open button. Hovering or moving keyboard focus never executes direct commands. Comparator retains its existing menu layout. Viewer's read-only status line keeps the sampling mode on the left and actual magnification on the far right, without duplicate top-level zoom or Fit/100% controls. Video frame/time progress remains available in its existing playback display. Help descriptions wrap and scroll for access to the full shortcut list. Native Windows window buttons and system dialogs retain their familiar behavior.

Windows RAW interpretation, copying/pasting image pixels, opening a file in Comparator and clearing a selection use shared availability checks for their existing entry points. Comparator source-format controls are native keyboard-accessible buttons for RAW input, with visible focus and disabled states; decoded-source format controls are disabled instead of offering an inapplicable action.

Viewer's autoplay preference applies to videos and multi-frame RAW files, both when opening directly and when moving to the previous or next file, without resizing the current window during navigation. Single-frame inputs, internal refreshes and rotating a paused source do not start playback. RAW resolution and color-space changes update the interpreted image and the visible menu labels. Unavailable commands use muted text rather than decorative dashed outlines; keyboard focus remains distinct from mouse activation.

Selection Mode is available in Options in both Windows apps, as well as through S and image context menus, with matching check states. Viewer's Sync Input Across Viewer Windows toggle is also available in Options and its context menu. Comparator retains shared pane interaction and the contextual Clear Selection/Esc action.

Windows Viewer supports native Y4M RAW-frame interpretation even for .yuv filenames: supported layouts retain editable resolution and color-space controls, frame-accurate seeking and autoplay, without treating container headers as pixels or changing the source file.

An empty Windows Viewer session opens with a roomier, DPI-scaled 800×600-class viewing area and measured menu clearance, limited to the monitor work area. The thumbnail drawer stays inside that window. The initial resolution for RAW inputs without recognized dimensions is 800×600 pixels; filename dimensions and a manually selected resolution remain supported.

Viewer's Options > Theme menu offers shared System, Light, and Dark choices for both Viewer and Comparator. Comparator follows automatically without a duplicate theme menu, including its Timeline scrollbar. One saved choice updates application colors and image-surround brightness together and respects Windows high contrast. Theme changes keep open media, zoom, selections, playback, and comparison results intact; image pixels and captured regions are not tinted. Windows-owned dialogs and popup chrome retain their OS appearance.

It is built for codec developers, imaging engineers, computer vision engineers, researchers, and QA teams who need to verify raw buffers, decoded frames, compression artifacts, color conversion, timing behavior, and small visual differences between processing pipelines.

Q1View includes two focused applications:

VIEWER

Open an image, raw frame dump, frame sequence, or video file. Zoom down to individual pixels, inspect exact coordinates, read component values, step through frames one at a time, and capture the current view or selected region.

For raw YUV sources, Viewer can show native Y, U, and V values sampled directly from the source planes, including subsampled and 10-bit formats, instead of only showing values reconstructed from the displayed RGB image. Press V to switch between source YUV values and display RGB values.

Raw input can be reinterpreted quickly without reopening the file. Press N to cycle color spaces and D to cycle preset resolutions. This is useful when debugging raw YUV/RGB dumps whose format, size, or color interpretation must be checked quickly.

Viewer also supports image-sequence navigation, video playback, frame/time progress display, clipboard paste, screen capture, and a resizable left-side thumbnail browser for browsing the current folder. Its GPU-rendered gallery smoothly animates thumbnail size changes while preserving selection and active playback; drag the divider to widen the browser without changing the window size. The Windows drawer automatically refreshes after files or folders are added, removed, or renamed in its current directory. In the Windows Viewer, select two to four media files with Ctrl+click, Shift+click, or Ctrl+A and compare them in Comparator; select any number of media files and move them to the Recycle Bin after a count-based confirmation, while folders and other files stay untouched. Open multiple Viewer windows and enable Sync Input to keep navigation, zoom, pan, rotation, playback, and display settings synchronized across them.

BT.2020 HLG videos are tone-mapped for accurate color and contrast on standard SDR Windows displays instead of being shown with a washed-out legacy color conversion.

The Windows Browse pane displays its current folder in a distinct, read-only path header above both list and gallery views. Long paths keep the current folder visible where space permits, with full-path tooltips and keyboard selection/copy. Directory listings are prepared before replacing the current view; failed navigation preserves the previous folder, selection, scroll and displayed image, with guidance for explicit failures and no repeated background-refresh dialogs.

In the Windows Viewer, right-click a single thumbnail to open it, copy the file or its path, reveal it in File Explorer, view Windows properties, or move it to the Recycle Bin; right-click a selection of two to four media files and choose Compare selected files to open them in Comparator, or move a larger selection to the Recycle Bin. Delete performs the same confirmed recycle operation. Gallery grids focus on useful image and video previews, while compact mode keeps the complete folder list. Directories use clean **[Folder name]** and **[..]** notation with balanced Pretendard typography instead of decorative folder artwork; long names wrap across the card and use an ellipsis only on the final visible line. Every non-empty folder, including a drive root, starts with a visible **[..]** entry; activating it from `D:\` opens the top-level logical-drive chooser so another volume such as `C:\` can be selected. Backspace and Alt+Up provide the same navigation without interrupting the photo or video in the main view, while Viewer shortcuts remain available when the drawer has focus.

Choose Auto, Smooth (Bilinear), or Pixel Exact (Nearest) image scaling from Options > Image Scaling. Auto keeps scanned text clear at moderate reductions, suppresses aliasing when large photos are reduced substantially, and preserves exact pixels during close inspection.

Windows Gallery folder and drive captions align at the top left, with the parent entry centered. Windows UI Automation exposes full item names, focus, selection and existing open/scroll actions to assistive tools. If moving the displayed file to the Recycle Bin fails, Viewer retains the image view and playback state; successful deletion keeps Browse keyboard focus and selects the next or previous surviving image, or clears the view when none remain.

COMPARATOR

Open two to four sources side by side. Comparator uses A as the fixed reference and calculates independent PSNR, SSIM, and LPIPS scores against B, C, and D. Compact results sit directly under each candidate without repeating filenames; separate sequence graphs keep each comparison clear, while Timeline shows up to four source lanes. Synchronized region selection adds crop scores for every comparison; missing frames are not padded with repeated measurements.

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
6. Video playback aligned to source or user-selected FPS with frame/time progress, stall recovery, and BT.2020 HLG-to-SDR tone mapping
7. Sync Input: link multiple Viewer windows for synchronized navigation
8. HEIF, HEIC, HIF, and AVIF still image support
9. Frame sequence navigation through same-folder images
10. Clipboard paste and screen capture support
11. Unicode and Korean path support
12. Launch the dedicated Comparator workflow directly from Viewer
13. Step through video frames one at a time with Left/Right arrow keys
14. Auto, Smooth, and Pixel Exact scaling modes with visible current-state feedback
15. Allow Different Resolution mode for cross-resolution comparisons
16. Portable ZIP available alongside the installer
17. Cycle raw color space (N) and preset resolutions (D) without reopening the file
18. Windows Viewer media thumbnail gallery with automatic directory refresh, multi-selection and comparison of two to four sources in Comparator, safe batch Recycle Bin actions, smooth sizing, bounded caching, compact folder notation, modern Korean/Latin typography, global shortcuts, and top-level logical-drive switching
19. Coordinated Viewer, Comparator, photo, video, and raw-file icons designed for clear recognition across Windows taskbar, Start, and File Explorer surfaces
20. Shared Korean/English typography, text-size scaling, and System/Light/Dark themes with matching image-surround brightness
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
