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

Then, press [Build] - [Build Solution]. Done. It is that easy.

## Cmake option for OpenCV 4.3.0
CMAKE_OPTIONS='-DBUILD_DOCS:BOOL=OFF -DWITH_CUDA:BOOL=OFF -DBUILD_EXAMPLES:BOOL=OFF  -DWITH_GSTREAMER:BOOL=OFF -DWITH_OPENCLAMDBLAS:BOOL=OFF -DBUILD_PERF_TESTS:BOOL=OFF -DWITH_OPENCLAMDFFT:BOOL=OFF -DBUILD_SHARED_LIBS:BOOL=OFF -DBUILD_TESTS:BOOL=OFF -DWITH_OPENCL_D3D11_NV:BOOL=OFF -DBUILD_PACKAGE:BOOL=OFF -DWITH_OPENCL:BOOL=OFF -DOPENCV_DNN_OPENCL:BOOL=OFF -DBUILD_WITH_STATIC_CRT:BOOL=OFF -DWITH_DIRECTX:BOOL=OFF -DWITH_DSHOW:BOOL=OFF  -DWITH_MSMF:BOOL=OFF  -DWITH_MSMF_DXVA:BOOL=OFF'

## How to use
- Detailed `help` menu description is shown when pressed `?`
- We can show & play almost all raw and encoded files. We are also able to show many (consecutive) image files as a video in a directory (`opencv_videoio_ffmpeg430_64.dll` file is required to be in the `PATH` directories)
- You can select image regions
- You can copy & paste to/from clipboard
- To support new raw formats, please change the `qimage_cs.h` file in the `QVisionCore` project

## TODOs and known problems
- TODOs and known problems are desribed in each Viewer and Comparer project.
- You can add your own functionalities by going through the `review` process.
- If you're interested in, you can also implement one of the TODO list.