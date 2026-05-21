cmake_minimum_required(VERSION 3.20)

project(Q1ViewCore LANGUAGES C CXX)

option(Q1VIEW_BUILD_SMOKE_TEST "Build the cross-platform core smoke-test executable" ON)
option(Q1VIEW_BUILD_QT_VIEWER "Build the experimental cross-platform Qt viewer" OFF)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(q1view_image_core STATIC
    QVisionCore/qimage_cs.c
    QVisionCore/qimage_metrics.c
    QVisionCore/qimage_util.c
)

target_include_directories(q1view_image_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/QCommon/inc
        ${CMAKE_CURRENT_SOURCE_DIR}/QVisionCore
)

if(UNIX AND NOT APPLE)
    target_link_libraries(q1view_image_core PUBLIC m)
endif()

add_library(smultithreadplus STATIC
    SMultithreadPlus/src/SBufferPool.cpp
    SMultithreadPlus/src/SCondition.cpp
    SMultithreadPlus/src/SMutex.cpp
    SMultithreadPlus/src/SParcel.cpp
    SMultithreadPlus/src/SReqListener.cpp
    SMultithreadPlus/src/SThread.cpp
)

target_include_directories(smultithreadplus
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/QCommon/inc
        ${CMAKE_CURRENT_SOURCE_DIR}/SMultithreadPlus/inc
)

if(WIN32)
    target_compile_definitions(smultithreadplus PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(smultithreadplus PUBLIC Threads::Threads)
endif()

if(Q1VIEW_BUILD_SMOKE_TEST)
    enable_testing()

    add_executable(q1view_core_smoke
        tests/core_smoke.cpp
    )

    target_link_libraries(q1view_core_smoke
        PRIVATE
            q1view_image_core
            smultithreadplus
    )

    add_test(NAME q1view_core_smoke COMMAND q1view_core_smoke)
endif()

if(Q1VIEW_BUILD_QT_VIEWER)
    find_package(Qt6 QUIET COMPONENTS Widgets)

    if(NOT Qt6_FOUND)
        find_package(Qt5 REQUIRED COMPONENTS Widgets)
        set(Q1VIEW_QT_LIB Qt5::Widgets)
    else()
        set(Q1VIEW_QT_LIB Qt6::Widgets)
    endif()

    add_executable(q1view_viewer_qt
        ViewerQt/main.cpp
        ViewerQt/MainWindow.cpp
        ViewerQt/MainWindow.h
    )

    set_target_properties(q1view_viewer_qt PROPERTIES
        MACOSX_BUNDLE TRUE
        WIN32_EXECUTABLE TRUE
    )

    target_link_libraries(q1view_viewer_qt
        PRIVATE
            ${Q1VIEW_QT_LIB}
            q1view_image_core
    )

    install(TARGETS q1view_viewer_qt
        BUNDLE DESTINATION .
        RUNTIME DESTINATION bin
    )
endif()
