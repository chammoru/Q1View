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
- QVisionCore refactor/namespace q1
- Prefix를 기존에 Qcv, Q 다양하게 붙였는데 통일 필요
- 파일이름도 QImage, QVision 다양하게 붙였는데 수정 필요

- Explain how to build
- The location of TODO files (if you're interested in, you can contribute)
- Explain how to support new raw formats (한중 코드 수정 방법)
- 알려진 문제들
- 영역선택하는 방법
- '?' 선택되었을 때 나오는 모든 메뉴
