# HEIF support

Q1View decodes HEIF/HEIC files through `q1::imreadW()`.

The normal OpenCV decoder is tried first. If OpenCV cannot decode the buffer and
the file is an ISO BMFF HEIF-style image, Q1View falls back to libheif when the
build is configured with `Q1VIEW_HEIF_ROOT`.

## Windows build

Set `Q1VIEW_HEIF_ROOT` to a libheif installation root that contains `include`,
`lib`, and `bin` directories. The GitHub Actions release build installs
`libheif:x64-windows` through vcpkg and sets this variable automatically.

Example local setup:

```powershell
vcpkg install libheif:x64-windows
$env:Q1VIEW_HEIF_ROOT = "$env:VCPKG_INSTALLATION_ROOT\installed\x64-windows"
msbuild Viewer\Viewer.sln /m /restore /p:Configuration=Release /p:Platform=x64
msbuild Comparer\Comparer.sln /m /restore /p:Configuration=Release /p:Platform=x64
```

Release packages must include the libheif runtime DLLs from
`$env:Q1VIEW_HEIF_ROOT\bin` next to `Viewer.exe` and `Comparer.exe`.
