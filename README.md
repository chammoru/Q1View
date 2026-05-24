# Q1View

[![Windows](https://img.shields.io/badge/platform-Windows-0078D6)](#download)
[![Viewer](https://img.shields.io/badge/app-Viewer-2EA44F)](#what-you-get)
[![Comparer](https://img.shields.io/badge/app-Comparer-2EA44F)](#what-you-get)
[![HEIF](https://img.shields.io/badge/HEIF%2FHEIC-supported-2EA44F)](#supported-inputs)
[![AVIF](https://img.shields.io/badge/AVIF%2FAV1-supported-2EA44F)](#supported-inputs)

Q1View is a Windows viewer and comparer for people who inspect pixels, decoded
frames, raw buffers, and codec output.

## Download

Get the latest Windows x64 build from the
[GitHub Releases page](https://github.com/chammoru/Q1View/releases/latest).

- **Installer**: `Q1ViewSetup-x64.exe`
- **Portable package**: `Q1View-windows-x64.zip`

The installer adds separate Start Menu entries for **Q1View Viewer** and
**Q1View Comparer**.

## What You Get

- **Viewer** opens images, raw dumps, image sequences, videos, and clipboard
  images with pixel-level inspection.
- **Comparer** compares 2-4 sources with synchronized navigation, zooming,
  color-space handling, PSNR, and SSIM.

## Why Q1View?

- Raw pixel formats are first-class: YUV, NV12/NV21, P010/P210, RGB/BGR/RGBA,
  grayscale, packed RGB, and more.
- Image folders can be treated like frame sequences.
- Exact pixel values, coordinates, luma-only view, nearest-neighbor zoom, and
  selected-region copy are built into the workflow.
- HEIF/HEIC/HIF and AVIF still images are supported in both Viewer and Comparer.
- The release package includes the DLLs it needs, so users do not need to
  install OpenCV or HEIF libraries separately.

Q1View is not trying to be a photo library. It is built for Windows pixel
debugging, frame comparison, and quick visual checks during codec or image
pipeline work.

## Supported Inputs

- BMP, JPEG, PNG, TIFF, WebP
- HEIF, HEIC, HIF
- AVIF / AV1 still images
- Video files supported by the packaged OpenCV/FFmpeg runtime
- Raw frame dumps and clipboard images

## Build From Source

Q1View builds with Visual Studio 2019 or newer on Windows x64.

```powershell
msbuild Viewer\Viewer.sln /m /restore /p:Configuration=Release /p:Platform=x64
msbuild Comparer\Comparer.sln /m /restore /p:Configuration=Release /p:Platform=x64
```

Normal builds restore OpenCV and libheif into `.deps` from published dependency
archives. Dedicated GitHub Actions workflows refresh those dependency archives
when needed.

## Release Pipeline

GitHub Actions builds Viewer and Comparer, stages the runtime files, creates the
portable zip, builds the Inno Setup installer, and publishes release assets for
version tags such as `v1.0.9`.

The pipeline also supports Authenticode signing when signing credentials are
configured through repository secrets.

## Project Layout

| Path | Purpose |
| --- | --- |
| `Viewer\` | Single-source viewer application |
| `Comparer\` | Multi-pane frame comparison application |
| `QVisionCore\` | Image loading, raw color conversion, HEIF/AVIF fallback, metrics |
| `build\` | Packaging, signing, and dependency helper scripts |
| `installer\` | Inno Setup installer definition |
| `docs\` | Focused technical notes |

## Videos

| Intro | Viewer | Comparer |
| --- | --- | --- |
| [![Intro](https://img.youtube.com/vi/b8VgRVnrxL4/mqdefault.jpg)](https://youtu.be/b8VgRVnrxL4) | [![Viewer](https://img.youtube.com/vi/g6K9bRTKJjY/mqdefault.jpg)](https://youtu.be/g6K9bRTKJjY) | [![Comparer](https://img.youtube.com/vi/EybIIBZLV8Q/mqdefault.jpg)](https://youtu.be/EybIIBZLV8Q) |

More videos:

- [Viewer 2](https://youtu.be/ROpGsgRDdRE)
- [Source code](https://youtu.be/ZbpRkBzK64Q)
- [TODOs and etc.](https://youtu.be/s2dngkHUFpQ)
