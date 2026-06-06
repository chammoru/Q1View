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
        ViewerQt/ImageView.cpp
        ViewerQt/ImageView.h
        ViewerQt/RawOpenDialog.cpp
        ViewerQt/RawOpenDialog.h
        ViewerQt/HeifReader.cpp
        ViewerQt/HeifReader.h
        ViewerQt/Y4mReader.cpp
        ViewerQt/Y4mReader.h
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

    # --- Optional HEIF/HEIC/AVIF support via libheif -----------------------
    # When found, the viewer decodes HEIF-family images directly (see
    # ViewerQt/HeifReader.cpp). When not, HEIF support silently turns off and
    # the rest of the viewer builds unchanged.
    #
    # Windows: default to the same prebuilt "decode-av1" root the MSBuild
    # product restores under .deps (see Q1ViewHeif.props). It ships an
    # include/lib/bin layout, so it is consumed directly rather than via its
    # vcpkg CMake package. Unix: discover libheif through pkg-config
    # (apt/brew install libheif).
    if(NOT DEFINED Q1VIEW_HEIF_ROOT AND DEFINED ENV{Q1VIEW_HEIF_ROOT})
        set(Q1VIEW_HEIF_ROOT "$ENV{Q1VIEW_HEIF_ROOT}")
    endif()
    if(WIN32 AND NOT Q1VIEW_HEIF_ROOT)
        set(_q1_heif_vendored
            "${CMAKE_CURRENT_SOURCE_DIR}/.deps/libheif-decode-av1-x64-windows")
        if(EXISTS "${_q1_heif_vendored}/include/libheif/heif.h")
            set(Q1VIEW_HEIF_ROOT "${_q1_heif_vendored}")
        endif()
    endif()

    set(_q1_heif_enabled OFF)
    if(Q1VIEW_HEIF_ROOT AND EXISTS "${Q1VIEW_HEIF_ROOT}/include/libheif/heif.h")
        find_library(Q1VIEW_HEIF_LIB
            NAMES heif libheif
            PATHS "${Q1VIEW_HEIF_ROOT}/lib"
            NO_DEFAULT_PATH)
        if(Q1VIEW_HEIF_LIB)
            target_include_directories(q1view_viewer_qt
                PRIVATE "${Q1VIEW_HEIF_ROOT}/include")
            target_link_libraries(q1view_viewer_qt PRIVATE "${Q1VIEW_HEIF_LIB}")
            set(_q1_heif_enabled ON)
            set(_q1_heif_bin "${Q1VIEW_HEIF_ROOT}/bin")
        endif()
    else()
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(Q1VIEW_HEIF IMPORTED_TARGET libheif)
            if(Q1VIEW_HEIF_FOUND)
                target_link_libraries(q1view_viewer_qt
                    PRIVATE PkgConfig::Q1VIEW_HEIF)
                set(_q1_heif_enabled ON)
            endif()
        endif()
    endif()

    if(_q1_heif_enabled)
        target_compile_definitions(q1view_viewer_qt PRIVATE Q1VIEW_ENABLE_LIBHEIF)
        message(STATUS "Q1View Qt viewer: HEIF/HEIC/AVIF support enabled")

        # Windows prebuilt roots ship heif.dll and its codec DLLs (libde265,
        # aom). Copy them next to the exe so the viewer runs from the build tree.
        if(WIN32 AND _q1_heif_bin AND EXISTS "${_q1_heif_bin}")
            file(GLOB _q1_heif_dlls "${_q1_heif_bin}/*.dll")
            if(_q1_heif_dlls)
                add_custom_command(TARGET q1view_viewer_qt POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        ${_q1_heif_dlls} "$<TARGET_FILE_DIR:q1view_viewer_qt>"
                    COMMAND_EXPAND_LISTS
                    VERBATIM)
            endif()
        endif()
    else()
        message(STATUS "Q1View Qt viewer: HEIF support disabled (libheif not found)")
    endif()

    install(TARGETS q1view_viewer_qt
        BUNDLE DESTINATION .
        RUNTIME DESTINATION bin
    )
endif()
