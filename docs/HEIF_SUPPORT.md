# HEIF support

Q1View decodes HEIF/HEIC files through `q1::imreadW()`.

The normal OpenCV decoder is tried first. If OpenCV cannot decode the buffer and
the file is an ISO BMFF HEIF-style image, Q1View falls back to libheif when the
build is configured with `Q1VIEW_HEIF_ROOT`.

## Windows build

For x64 builds, Q1View enables HEIF support automatically. If `Q1VIEW_HEIF_ROOT`
is not set, MSBuild restores a prebuilt HEIF dependency archive into
`.deps\libheif-decode-av1-x64-windows`.

`Q1VIEW_HEIF_ROOT` can still be set manually to a libheif installation root that
contains `include`, `lib`, and `bin` directories.

Default archive:
- GitHub release tag: `deps-libheif-decode-av1-x64-windows`
- Asset: `Q1View-libheif-decode-av1-x64-windows.zip`

Overrides:
- `Q1VIEW_HEIF_ROOT`: use an already extracted libheif install root.
- `Q1VIEW_HEIF_ARCHIVE`: use a local dependency zip.
- `Q1VIEW_HEIF_URL`: use a custom dependency zip URL.
- `Q1VIEW_HEIF_SHA256`: verify the zip with a pinned SHA256.

To create or refresh the dependency archive, run the **Build HEIF Dependency**
workflow manually. It installs `libheif` through vcpkg with default features
disabled and the `aom` feature enabled, uploads the zip to a GitHub release, and
writes the SHA256 in the release notes. That keeps HEVC decoding through
libde265, adds AV1-backed AVIF decoding through aom, and excludes the x265 HEVC
encoder DLL.

Example local setup:

```powershell
msbuild Viewer\Viewer.sln /m /restore /p:Configuration=Release /p:Platform=x64
msbuild Comparator\Comparator.sln /m /restore /p:Configuration=Release /p:Platform=x64
```

The build copies the libheif runtime DLLs from `Q1VIEW_HEIF_ROOT\bin` next to
`Viewer.exe` and `Comparator.exe`.
