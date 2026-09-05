# Gallery grid validation (#81)

This branch adds a Direct2D/D3D11 gallery canvas to the Windows MFC Viewer. The compact list remains a CListCtrl. Qt is outside the scope of this Windows rendering issue. This is a test build; #81 remains open until the physical-display checks below are complete.

## Implementation

- Ctrl+wheel retains the existing list / 5 / 4 / 3 / 2 / 1-column choices. Grid-to-grid transitions last approximately 190 ms and retarget from the currently displayed rectangles when input reverses or repeats.
- Grid zoom does not enumerate the folder again, replace its entries, invalidate the folder generation, or rebuild a CImageList. Only visible transition tiles have stored animation rectangles.
- Decoded thumbnails are reused while zooming. Higher-resolution replacements are requested after input settles, with visible items before two look-ahead rows. Decode output is capped at 1024 pixels per side.
- CPU bitmap storage is limited to 64 MiB and 512 entries (also bounding GDI handles for tiny thumbnails). GPU bitmaps share the same cache-entry lifetimes and cannot exceed another 64 MiB of source pixels. At most four thumbnails can be decoding or waiting for UI consumption. Decoder-internal buffers, render targets and driver allocations are additional memory, not part of these cache budgets.
- GPU uploads are limited to two thumbnails per paint. The gallery uses nonblocking Present without waiting for another vertical sync on the video's UI thread. If GPU initialization/rendering fails, the canvas uses a GDI fallback; CPU thumbnails survive device recreation.
- Selection, keyboard navigation, pointer hit testing, hover and DPI-scaled borders belong to the canvas. File activation still uses the existing deferred Viewer document-opening path and fixed-window behavior.
- The gallery continues to omit folder entries and preserve extension badges for non-previewable files. Parent navigation and context actions remain #99; resolving its hidden-folder selection requirement is deferred to that issue.

## Automated checks

Run from the repository root with Visual Studio 2022/MSVC v143:

```powershell
msbuild Tests/GalleryLayoutTests.vcxproj /m /p:Configuration=Release /p:Platform=x64
./Tests/bin/x64/Release/GalleryLayoutTests.exe
msbuild Viewer/Viewer.sln /m /restore /p:Configuration=Release /p:Platform=x64 /p:Q1ViewGalleryTests=true
$env:Q1VIEW_GALLERY_TEST_REPORT = "$env:TEMP/gallery-integration.txt"
# Optional: an absolute path to an existing representative video. With this
# unset, the test creates its own 640x360 59.94fps MJPEG video.
# $env:Q1VIEW_GALLERY_TEST_VIDEO = 'D:\media\representative.mp4'
$test = Start-Process ./Viewer/x64/Release/Viewer.exe -ArgumentList '"Tests/fixtures/sample_16x16.png"' -PassThru
$test.WaitForExit()
Get-Content $env:Q1VIEW_GALLERY_TEST_REPORT
```

The integration build runs a real Viewer window using an isolated `Q1ViewGalleryTests` registry root. It creates uniquely named temporary fixtures and a report, checks a 3,000-file folder, interruption/selection, keyboard navigation, EOF scrolling/loading, list/grid switching, cache eviction, device recreation, GDI fallback/recovery, rapid folder changes, deferred activation, and actual playback during grid changes, drawer toggles and resizing. It enables the existing playback counters and reports presentation rate and frame intervals. Timing is observed at the application's message loop, not at physical monitor scanout. Initial/slow thumbnail loading uses a bounded completion wait, not a one-second deadline.

Build again with `/p:Q1ViewGalleryTests=false` before distributing Viewer. Normal builds do not include or run the integration-test driver. `Gallery validation build` in GitHub Actions produces normal portable binaries and a separate test report; it does not tag a release or submit anything to Microsoft Store.

## Local observations

On the available 1080p Windows machine, core regression tests and layout tests passed. Layout evaluation of 10,000 simulated frames took approximately 0.008 ms/frame with both 1,000 and 1,000,000 folder entries, demonstrating work proportional to the visible region rather than folder size.

Using the existing C0610_main.mp4 representative 59.94fps video, the final measured integration run reported baseline 58.99 presented fps (frame-interval p95 21.98 ms, p99 28.65 ms, maximum 42.88 ms) and continuous grid-size changes at 59.98 presented fps (p95 20.42 ms, p99 22.01 ms, maximum 22.67 ms). The test dispatches messages in bounded batches so queued playback messages cannot indefinitely delay synthetic zoom inputs. These are measurements from one local run, not a guarantee for every GPU/video/display combination. The baseline-to-stress comparison did not show a playback throughput regression in that run.

Computer Use recovered after reloading VS Code and captured the actual Viewer and compact drawer successfully, but its native pipe became unavailable again later. Consequently, automated renderer/state checks are stronger than the current direct visual coverage; do not treat the above timing results as proof that every transient display artifact is absent.

## Physical-display acceptance checklist

- [ ] On the company 4K monitor, test every grid size and fast Ctrl+wheel reversals. Confirm no jumping, blank flashes, incorrect clicked items, or stale selection/hover borders.
- [ ] Move between monitors using different Windows scale factors (for example 100%, 150%, 200%). Check gutters, selection thickness, tooltips, scroll position and hit testing.
- [ ] During the representative 59.94fps long-GOP video, repeat grid zoom, scrolling, E open/close, and divider resizing. Check subject position, image zoom and visible frame pacing; use high-frame-rate capture if shaking or incomplete frames recur.
- [ ] Check cold/warm folders with thousands of real photos/videos, missing/unreadable files, and rapid folder changes, including memory behavior over an extended session.
- [ ] Check synchronized Viewer instances: browsing or zooming a drawer must not replace, seek or pause the active media in either instance.
- [ ] Check light/dark Windows settings and the normal app palette, minimize/restore, and GPU/display reconnection.

Record the artifact commit, display resolution/scaling, GPU/driver, video properties and any failing sequence. Keep #81 open until these checks have been completed; a linked verification issue may track work that requires the company display.
