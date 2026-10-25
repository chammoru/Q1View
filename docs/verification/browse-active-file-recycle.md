# Browse: recycling the displayed file

Scope: Windows Viewer only. Use generated temporary fixtures, not the user's open photograph. No release, system-setting changes or Narrator testing is part of this change.

## Problems and implementation

- The previous recycle path closed the document before asking Windows to move the files. If the operation failed, it reopened the active file as a fresh, fitted image, losing manual zoom/pan, rotation, selection regions and the playback position/state.
- Release decoder/audio/file handles and stop the file watcher without destroying the last presented RGB buffer or document/view metadata. If recycling fails, reopen the same source while keeping that buffer reserved, retain the current interpretation and frame, and resume only an existing playback intent. A paused video remains paused even if automatic playback is enabled; existing manual playback resumes even if automatic playback is disabled.
- After successful deletion, use the same in-place document navigation route as Page Up/Down, rather than reactivating the MFC frame/view. Keep Browse keyboard focus so another Delete still targets Browse.
- Keep the existing next-survivor, previous-survivor, then empty-view policy. Explicitly update empty-view status so the deleted image's zoom/sampling text does not remain visible.
- Preserve the document-generation guard: a different file opened during the Shell operation must not be overwritten by its completion.

## Regression coverage

`Tests/GalleryIntegrationTests.cpp`, `RecycleTests`, uses the real MFC pane and Windows Shell Recycle Bin operation. Only generated fixture files are submitted; tests replace the app confirmation callback and suppress error dialogs for their intentionally locked files.

- List and Gallery: failed deletion of a rotated, manually zoomed/panned image retains its exact RGB buffer, ROI and selection. The 16x16 fixture is enlarged beyond the viewport so pan is meaningful rather than automatically centered.
- List and Gallery: successful active-image deletion chooses the next survivor, then the previous survivor, without resizing; Browse retains keyboard focus.
- List and Gallery: deleting the final image clears document/pixels and changes zoom to `--` with empty sampling text.
- List and Gallery: failed deletion of a paused video retains a nonzero frame and does not autoplay; the restored decoder can step and play again.
- List and Gallery: failed deletion of a manually playing video resumes beyond the retained frame with the new-file autoplay preference disabled. Successful video deletion releases its handles and opens the surviving image.
- Compact list: failed RAW deletion retains frame, dimensions, color space and frame count, and subsequent frame reads work. Successful RAW deletion releases the file handle and selects the previous image. RAW remains intentionally excluded from visual Gallery tiles.
- Retain existing cancellation, multi-selection, locked-file partial failure, Unicode filenames, directory/permanent-delete rejection, and stale thumbnail/watcher notification checks.

## Local verification

Run the instrumented Release/x64 Viewer with `Q1VIEW_GALLERY_TEST_REPORT` set to a report path and `Q1VIEW_BROWSER_NAVIGATION_ONLY=1`. The tests create an actual visible viewport before asserting presented-image state.

Evidence paths (ignored build outputs):

- `Tests/bin/x64/Release/browse-recycle-after.txt`: focused Browse navigation and recycle integration results.
- `Tests/bin/x64/Release/browse-recycle-after-build.log`: instrumented Viewer build.
- `Tests/bin/x64/Release/browse-recycle-release-build.log`: ordinary Viewer Release build, local version `0.0.0-dev`.

Also run `CoreRegressionTests.exe` and `GalleryLayoutTests.exe`. Validation results are recorded in the change handoff; historical intermediate failing reports are not acceptance evidence.

### Accepted local results — 2026-10-25

- Focused Browse navigation/recycling: two consecutive runs, each **391 PASS**, native process exit code **0**. Reports: `browse-recycle-after.txt` and `browse-recycle-repeat.txt`.
- Instrumented Viewer Release/x64/v143 build: passed. The reused test-only intermediate directory retains its existing MSB8028 shared-directory warning.
- Ordinary Viewer Release/x64/v143 build: passed, executable version `0.0.0-dev`.
- Core regression project build and executable: passed. Gallery layout executable: passed (1,000 and 1,000,000 entries).
- `git diff --check`: passed. No user media was deleted; no merge, push or release was performed.
