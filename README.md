# Q1View

[![Windows](https://img.shields.io/badge/platform-Windows-0078D6)](#build)
[![C++](https://img.shields.io/badge/language-C%2B%2B-00599C)](#project-layout)
[![OpenCV](https://img.shields.io/badge/OpenCV-4.3.0-5C3EE8)](#dependencies)
[![HEIF](https://img.shields.io/badge/HEIF%2FHEIC-supported-2EA44F)](#features)

Q1View is a Windows image, raw-frame, and video-frame viewer built for people
who debug pixels rather than only browse photos.

It includes two desktop tools:

- **Viewer**: open images, raw dumps, image sequences, videos, and clipboard
  images with pixel-level inspection.
- **Comparer**: compare 2-4 frame sources with synchronized navigation,
  zooming, color-space handling, and quality metrics.

## Why Q1View?

Most image viewers are optimized for photo browsing. Q1View is more focused on
engineering workflows: checking decoded frames, validating raw buffers,
comparing codec output, and inspecting exact pixel values.

Strengths that make it different:

- **Raw pixel formats are first-class**: YUV420, NV12, NV21, P010, P210,
  YUV 10-bit formats, RGB/BGR/RGBA variants, grayscale, and packed RGB formats.
- **Viewer and comparer live together**: inspect a single frame source, then use
  the comparer for side-by-side or 2-4 pane analysis.
- **Frame-oriented navigation**: image folders can be treated like frame
  sequences, and video/frame sources can be stepped, played, or seeked.
- **Pixel-level tooling**: zoom in to see pixel values, switch decimal/hex
  display, show cursor coordinates, toggle luma-only view, and copy selected
  regions to the clipboard.
- **Comparison metrics**: comparer calculates PSNR and SSIM and visualizes
  per-frame trends for RGB/YUV sources.
- **Modern image coverage where it matters**: common OpenCV formats plus
  HEIF/HEIC/HIF and AVIF still images through libheif in both Viewer and
  Comparer.
- **Reproducible dependency flow**: OpenCV and libheif are restored from pinned
  prebuilt dependency archives instead of being rebuilt on every local or CI
  run.

## Honest Tradeoffs

Q1View is not trying to be a polished cross-platform photo gallery.

- It is currently **Windows-only** and built with Visual Studio/MFC.
- The UI is practical and dense, not a modern gallery-style interface.
- OpenCV is still pinned to **4.3.0** for compatibility.
- Clean CI/local builds depend on the published OpenCV and HEIF dependency
  release assets. Refreshing those assets is a separate maintenance workflow.
- Documentation is improving, but the project still needs better screenshots,
  release notes, and onboarding for new contributors.

If you need a general-purpose, cross-platform image organizer, Q1View is not the
best fit. If you need to inspect raw frames, compare decoder output, or debug
pixel data on Windows, Q1View is built for that job.

## Features

- Open BMP, JPEG, PNG, TIFF, WebP, HEIF/HEIC/HIF, AVIF, videos, raw dumps, and
  clipboard images.
- Treat consecutive image files in a directory as a playable frame sequence.
- Drag and drop files into Viewer or Comparer.
- Navigate frames/files with keyboard shortcuts.
- Zoom with nearest-neighbor or interpolation modes.
- Inspect exact pixel values when zoomed in.
- Toggle decimal/hex pixel value display.
- Toggle luma-only view for quick luminance inspection.
- Select regions and copy either the full view or selected region to clipboard.
- Compare 2-4 sources side by side.
- Calculate PSNR and SSIM for frame comparisons.
- Extend raw format support in `QVisionCore/qimage_cs.h`.

Press `?` in Viewer to show the built-in shortcut panel.

## Build

Q1View builds with Visual Studio 2019 or newer on Windows x64.

Open and build these solutions:

| Tool | Solution |
| --- | --- |
| Viewer | `Viewer\Viewer.sln` |
| Comparer | `Comparer\Comparer.sln` |

Command-line build:

```powershell
msbuild Viewer\Viewer.sln /m /restore /p:Configuration=Release /p:Platform=x64
msbuild Comparer\Comparer.sln /m /restore /p:Configuration=Release /p:Platform=x64
```

## Dependencies

Normal builds restore dependencies into `.deps`:

- OpenCV 4.3.0 from `deps-opencv-4.3.0-msvc-x64-static-mt`
- libheif from `deps-libheif-decode-av1-x64-windows`

The OpenCV archive is intentionally trimmed for Q1View's current needs. It keeps
the `core`, `imgproc`, `imgcodecs`, `videoio`, and `highgui` modules and the
runtime/static libraries needed by the apps, while unused pieces such as DNN,
object detection, QR decoding, OpenCV apps, and protobuf are left out. This keeps
the dependency artifact smaller and avoids rebuilding old OpenCV code on every
CI run.

The HEIF archive is produced with vcpkg using libheif's default features
disabled and the `aom` feature enabled. It contains libheif plus the runtime
DLLs Q1View needs for HEVC-backed HEIF/HEIC/HIF decoding through libde265 and
AV1-backed AVIF decoding through aom. It intentionally excludes x265 because
Q1View does not encode HEIF images.

Override variables:

- `Q1VIEW_OPENCV_ROOT`: use an already extracted OpenCV install root.
- `Q1VIEW_OPENCV_ARCHIVE`: use a local OpenCV dependency zip.
- `Q1VIEW_OPENCV_URL`: use a custom OpenCV dependency zip URL.
- `Q1VIEW_OPENCV_SHA256`: verify the OpenCV zip with a pinned SHA256.
- `Q1VIEW_HEIF_ROOT`: use an already extracted libheif install root.
- `Q1VIEW_HEIF_ARCHIVE`: use a local libheif dependency zip.
- `Q1VIEW_HEIF_URL`: use a custom libheif dependency zip URL.
- `Q1VIEW_HEIF_SHA256`: verify the libheif zip with a pinned SHA256.

To create or refresh dependency archives, run these GitHub Actions workflows
manually:

- **Build OpenCV Dependency**
- **Build HEIF Dependency**

## CI/CD

GitHub Actions builds the Windows x64 release package.

- Pushes and pull requests build Viewer and Comparer as `Release|x64`.
- Normal CI downloads the prebuilt OpenCV and HEIF archives. The dependency
  source builds are only run when their dedicated workflows are triggered.
- Tags such as `v1.0.0` create a GitHub release.
- Release packages include `Viewer.exe`, `Comparer.exe`, required DLLs, and
  `Q1View-windows-x64.zip`.
- The release body and package include a generated `CHANGELOG.md`.

Recent hosted CI timing on `windows-2022`:

| Workflow | What it does | Time |
| --- | --- | --- |
| Build and Release | Restore dependency zips, build Viewer and Comparer, upload package | about 5-6 min |
| Build OpenCV Dependency | Build/package the trimmed OpenCV 4.3.0 archive | about 6 min 45 sec |
| Build HEIF Dependency | Build/package libheif plus the AV1 codec through vcpkg | about 11 min |

Keeping OpenCV and HEIF as dependency release assets avoids adding roughly
7-9 minutes to every normal CI run compared with the previous on-demand
dependency source-build path.

Manual release:

1. Open the repository's **Actions** tab.
2. Run **Build and Release**.
3. Set `create_release` to `true`.
4. Enter a tag name such as `v1.0.0`.

## Project Layout

| Path | Purpose |
| --- | --- |
| `Viewer\` | Single-source viewer application |
| `Comparer\` | Multi-pane frame comparison application |
| `QVisionCore\` | Image loading, raw color conversion, HEIF fallback, metrics |
| `QCommon\` | Shared utility code |
| `build\` | Dependency restore/build helper scripts |
| `docs\` | Focused technical notes |

## Videos

| Intro | Viewer | Comparer |
| --- | --- | --- |
| [![Intro](https://img.youtube.com/vi/b8VgRVnrxL4/mqdefault.jpg)](https://youtu.be/b8VgRVnrxL4) | [![Viewer](https://img.youtube.com/vi/g6K9bRTKJjY/mqdefault.jpg)](https://youtu.be/g6K9bRTKJjY) | [![Comparer](https://img.youtube.com/vi/EybIIBZLV8Q/mqdefault.jpg)](https://youtu.be/EybIIBZLV8Q) |

More videos:

- [Viewer 2](https://youtu.be/ROpGsgRDdRE)
- [Source code](https://youtu.be/ZbpRkBzK64Q)
- [TODOs and etc.](https://youtu.be/s2dngkHUFpQ)

## Status

Q1View is actively maintained, but still has rough edges. The most useful next
improvements would be:

- Add screenshots or short GIFs to make the workflow clear at a glance.
- Improve contributor documentation for adding raw formats and metrics.
- Add stricter SHA256 pinning for the published dependency archives.
- Continue modernizing the build while keeping the pixel-debugging workflow fast.
