# Q1View

## Project description
A developer-friendly media viewer

## build environment
- Visual Sutdio 16 2019, Win64
- OpenCV 4.3.0

## What to open first
| Sub-project |  Solution file        |
| ----------- |  ---------------------|
| Comparer    |  Comparer\Comparer.sln|
| Viewer      |  Viewer\Viewer.sln    |


## Cmake option for OpenCV 4.3.0
CMAKE_OPTIONS='-DBUILD_DOCS:BOOL=OFF -DWITH_CUDA:BOOL=OFF -DBUILD_EXAMPLES:BOOL=OFF  -DWITH_GSTREAMER:BOOL=OFF -DWITH_OPENCLAMDBLAS:BOOL=OFF -DBUILD_PERF_TESTS:BOOL=OFF -DWITH_OPENCLAMDFFT:BOOL=OFF -DBUILD_SHARED_LIBS:BOOL=OFF -DBUILD_TESTS:BOOL=OFF -DWITH_OPENCL_D3D11_NV:BOOL=OFF -DBUILD_PACKAGE:BOOL=OFF -DWITH_OPENCL:BOOL=OFF -DOPENCV_DNN_OPENCL:BOOL=OFF -DBUILD_WITH_STATIC_CRT:BOOL=OFF -DWITH_DIRECTX:BOOL=OFF -DWITH_DSHOW:BOOL=OFF  -DWITH_MSMF:BOOL=OFF  -DWITH_MSMF_DXVA:BOOL=OFF'

## TODO
- Explain how to build
- The location of TODO files (if you're interested in, you can contribute)
- Explain how to support new raw formats (한줄 코드 수정 방법)
- Explain the known problems
- Explain how to select regions
- Detailed 'help' menu description shown when pressed '?'
