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
    # Portable image-string helpers (resolution/FPS/colour-space label parsing)
    # shared by the MFC front-ends and the Qt viewer. QImageStr.cpp no longer
    # pulls in the OpenCV-heavy QViewerCmn.h, so it compiles cleanly here.
    QVisionCore/QImageStr.cpp
)

target_include_directories(q1view_image_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/QCommon/inc
        ${CMAKE_CURRENT_SOURCE_DIR}/QVisionCore
)

if(UNIX AND NOT APPLE)
    target_link_libraries(q1view_image_core PUBLIC m)
endif()

# QImageStr.cpp uses std::stod, which can throw; make sure C++ unwind semantics
# are enabled so exceptions propagate correctly (and to silence MSVC C4530).
if(MSVC)
    target_compile_options(q1view_image_core PRIVATE /EHsc)
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
    # Network backs the cross-platform multi-window Sync Input bus (ViewerQt/
    # SyncChannel.cpp); it ships in qtbase, so it is a hard requirement.
    find_package(Qt6 QUIET COMPONENTS Widgets Network)

    if(NOT Qt6_FOUND)
        # 5.15 is the floor: the viewer uses C++17 (Qt 5.15 supports it) and
        # SyncChannel pins QDataStream::Qt_5_15, an enumerator that doesn't exist
        # in older Qt5. Pinning the minimum fails clearly at configure time
        # instead of obscurely at compile time on an older Qt5.
        find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets Network)
        set(Q1VIEW_QT_LIB Qt5::Widgets Qt5::Network)
    else()
        set(Q1VIEW_QT_LIB Qt6::Widgets Qt6::Network)
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
        # Side thumbnail/folder drawer (the cross-platform CThumbnailPane).
        ViewerQt/ThumbnailPane.cpp
        ViewerQt/ThumbnailPane.h
        # Multi-window Sync Input over loopback UDP multicast (replaces the
        # Windows WM_COPYDATA broadcast).
        ViewerQt/SyncChannel.cpp
        ViewerQt/SyncChannel.h
        # Qt resource bundle: ships the window/taskbar icon (see main.cpp).
        ViewerQt/q1view.qrc
    )

    # The .qrc above is compiled by rcc; on Windows the .rc also stamps the icon
    # onto the executable file itself (shown by Explorer and the taskbar).
    # AUTOMOC drives SyncChannel/VideoView's Q_OBJECT signals.
    set_target_properties(q1view_viewer_qt PROPERTIES
        MACOSX_BUNDLE TRUE
        WIN32_EXECUTABLE TRUE
        AUTOMOC ON
        AUTORCC ON
    )

    if(WIN32)
        target_sources(q1view_viewer_qt PRIVATE ViewerQt/q1view.rc)
    endif()

    target_link_libraries(q1view_viewer_qt
        PRIVATE
            ${Q1VIEW_QT_LIB}
            q1view_image_core
    )

    # The viewer sources use C++17 (std::clamp, digit separators in
    # SyncChannel). Qt6 already requires C++17 and propagates it, but the global
    # CMAKE_CXX_STANDARD is 11 for the C core, so the Qt5 fallback would build at
    # C++11 and fail. Require C++17 on this target explicitly (Qt 5.15 supports
    # it) so both the Qt6 and Qt5 paths compile.
    target_compile_features(q1view_viewer_qt PRIVATE cxx_std_17)

    # --- Optional video playback via Qt6::Multimedia -----------------------
    # When the Multimedia / MultimediaWidgets modules are present, the viewer
    # plays video files on a QMediaPlayer/QVideoWidget page (ViewerQt/
    # VideoView.cpp). When absent (e.g. a Widgets-only Qt), video support
    # silently turns off and the rest of the viewer builds unchanged.
    # windeployqt/macdeployqt/linuxdeploy bundle the multimedia plugins when the
    # module is linked, so no extra packaging step is needed here.
    if(Qt6_FOUND)
        find_package(Qt6 QUIET COMPONENTS Multimedia MultimediaWidgets)
        if(Qt6Multimedia_FOUND AND Qt6MultimediaWidgets_FOUND)
            target_sources(q1view_viewer_qt PRIVATE
                ViewerQt/VideoView.cpp
                ViewerQt/VideoView.h)
            target_link_libraries(q1view_viewer_qt PRIVATE
                Qt6::Multimedia Qt6::MultimediaWidgets)
            target_compile_definitions(q1view_viewer_qt PRIVATE
                Q1VIEW_ENABLE_QT_MULTIMEDIA)
            message(STATUS "Q1View Qt viewer: video playback enabled (Qt6::Multimedia)")
        else()
            message(STATUS "Q1View Qt viewer: video playback disabled (Qt6::Multimedia not found)")
        endif()
    endif()

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

    # --- Minimal Qt smoke check --------------------------------------------
    # Drive the built viewer headlessly (--selftest under the offscreen QPA
    # platform) over the committed raw-format fixtures, asserting each opens
    # without crashing. This guards the shared-core decode paths through the Qt
    # front-end on every CI runner. Regenerate the fixtures with
    # Tests/fixtures/gen_raw_fixtures.c (see its header comment).
    enable_testing()

    # "<format> <fixture-extension>" for each representative raw fixture; all are
    # 16x16. The extension matches the color-space name, following the existing
    # *.yuv420 / *.rgb888 file-dialog convention.
    set(_q1_smoke_formats
        yuv420
        nv12
        nv21
        yuv420p10le
        p010
        grayscale
        rgb888
        bgr888
        rgba8888
        bgr565
    )

    foreach(_fmt IN LISTS _q1_smoke_formats)
        set(_fixture
            "${CMAKE_CURRENT_SOURCE_DIR}/Tests/fixtures/raw/gradient_16x16.${_fmt}")
        add_test(NAME q1view_qt_smoke_${_fmt}
            COMMAND q1view_viewer_qt --selftest --raw
                --width 16 --height 16 --format ${_fmt} "${_fixture}")
        # Offscreen so it runs without a display server on every CI runner; the
        # timeout keeps a failure (which can pop a modal warning) from hanging.
        set_tests_properties(q1view_qt_smoke_${_fmt} PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
            TIMEOUT 30)
    endforeach()
endif()
