# Q1View

## Project description
A developer-friendly media viewer

## Build environment
- Visual Sutdio 16 2019, Win64
- OpenCV 4.3.0

## Explain how to build
Open the next solution file for each Viewer and Comparer project.
| Sub-project |  Solution file         | Project Directory               |
| ----------- |  --------------------- | ------------------------------- |
| Viewer      |  Viewer\Viewer.sln     | [Viewer\\](Viewer/README.md)      |
| Comparer    |  Comparer\Comparer.sln | [Comparer\\](Comparer/README.md)  |

Then, press F7 button. Done. It is that easy.

## Cmake option for OpenCV 4.3.0
CMAKE_OPTIONS='-DBUILD_DOCS:BOOL=OFF -DWITH_CUDA:BOOL=OFF -DBUILD_EXAMPLES:BOOL=OFF  -DWITH_GSTREAMER:BOOL=OFF -DWITH_OPENCLAMDBLAS:BOOL=OFF -DBUILD_PERF_TESTS:BOOL=OFF -DWITH_OPENCLAMDFFT:BOOL=OFF -DBUILD_SHARED_LIBS:BOOL=OFF -DBUILD_TESTS:BOOL=OFF -DWITH_OPENCL_D3D11_NV:BOOL=OFF -DBUILD_PACKAGE:BOOL=OFF -DWITH_OPENCL:BOOL=OFF -DOPENCV_DNN_OPENCL:BOOL=OFF -DBUILD_WITH_STATIC_CRT:BOOL=OFF -DWITH_DIRECTX:BOOL=OFF -DWITH_DSHOW:BOOL=OFF  -DWITH_MSMF:BOOL=OFF  -DWITH_MSMF_DXVA:BOOL=OFF'

## Todo
- Explain How to use
  - Detailed 'help' menu description shown when pressed '?'
  - Show & Play all raw and encoded files. (Even many image files in a directory)
  - Explain how to select regions
  - Copy&paste to/from clipboard
- The location of TODO files (if you're interested in, you can contribute)
- Explain how to support new raw formats (한줄 코드 수정 방법)
- Explain the TODO list that you might contribute
- Explain the known problems