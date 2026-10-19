# Issue #121 — Browse path and transactional navigation

## Scope and approval — 2026-10-19

The user approved implementing the reduced [issue #121](https://github.com/chammoru/Q1View/issues/121) design. Work starts from `origin/master` `155844e2e260bc59d7a943f71b191ace5b0a5b78` on `codex/issue-121-browse-path-safety`. The scope is the Windows MFC Viewer's current-folder line and safe directory navigation. Existing parent/drive/UNC navigation, image transforms, source interpretation and autoplay remain contracts. Comparator and Qt are unchanged. No merge, release or issue closure is authorized by this implementation step.

## Implementation

- A read-only native Edit occupies the existing report control's header allocation, above both compact rows and the grid canvas. This retains the splitter, thumbnail coordinates within the canvas and document/view routing. Header height follows the current font metrics and DPI padding. Theme-aware header/edit painting avoids native white header bands.
- The native edit retains the exact committed Browse directory as its value. Unfocused painting middle-elides ancestors while preserving the drive/UNC identity and current leaf where space permits. A long leaf is itself elided without splitting UTF-16 surrogate pairs. Focus exposes selectable, horizontally scrollable full text; tooltip and MSAA retain the full string. Dynamic annotation names the edit `Current folder` and is cleared at destruction. Initial `No folder`, actual `Drives`, filesystem roots and normal folders are distinct states.
- Tab from the list/grid focuses the path. Escape returns to the same browser presentation without navigating or leaving fullscreen. Ctrl+A/C select/copy the path, and native caret/edit keys cannot turn into image commands. E, application accelerators and parent shortcuts are retained. Frame-level handling is necessary because MFC splitter/accelerator routing can consume these keys before child PreTranslateMessage.
- Directory metadata is prepared off the UI thread, never by decoding images. A directory handle validates list access; wildcard exhaustion, an accessible empty folder, access denial, missing targets and mid-enumeration failure are distinguished. The previous entries, selection, scroll, thumbnail generations and watcher remain intact until successful publication. Explicit failures report a target, reason and next action through the existing standard message-dialog path. Background failures remain silent.
- One metadata worker has a bounded latest-request mailbox. Superseded results/errors are discarded. Supported synchronous device/UNC operations are cancelled when superseded. Shutdown stops publication under the mailbox lock, cancels synchronous IO, drains notifications and detaches the potentially blocked worker; only shared mailbox/request data survives, never a pane pointer. No UI thread waits for an indefinitely blocked metadata call to return.
- A successful new-folder commit registers its watcher and validates once more to cover changes between enumeration and watch registration. Identical validation/refresh snapshots do not rebuild the list or invalidate thumbnails. Watch changes received during preparation are drained after completion, and current selection is captured at refresh publication, not overwritten with stale selection from request time. View-size changes reuse committed metadata.
- Explicit navigation invalidates deferred clicks on the old list; background refresh preserves the existing same-folder activation contract. A file action on the retained list pins that list and supersedes pending metadata rather than silently dropping Delete. Navigation during recycle confirmation invalidates the old batch before any recycle operation. Existing drive choices keep the root backslash in both rendered presentations.

## Regression fixes made during verification

The first ordinary-window observation exposed native white header bands. The header now paints the entire background using its theme role. A later observation showed that combining native end/path ellipsis flags could hide the current leaf; measured middle compaction now has direct drive/Unicode/UNC and narrow-width assertions.

Native Tab initially moved to the image pane instead of the path. Direct child-handler tests were insufficient: frame-level input routing was corrected and a real posted-message-pump assertion was added. Ctrl+A/C is also verified through the application's actual PreTranslateMessage path, including exact Unicode clipboard contents and restoration of the previous clipboard.

The asynchronous conversion exposed tests that assumed NavigateTo/GoToParent had already replaced the list when they returned. These tests now await actual bounded completion while retaining their original folder, selection, source and playback assertions. The existing no-argument full test launch also needed to establish a repository image before its loaded-source zoom/status assertions; empty-startup assertions still run separately.

A fixed 200 ms physical-E pump failed while a posted E key was still waiting in the queue and the folder request remained pending. Queue inspection confirmed unprocessed input, unchanged document, and no held Ctrl/Alt modifiers. The test now awaits the actual toggle with its existing bounded timeout, settles further messages, and still verifies one toggle per keypress, unchanged file, and no auto-repeat retoggle. No E production behavior was weakened to make the test pass.

Full interaction checks additionally caught Delete being ignored during asynchronous background refresh. Pinning the visible snapshot corrected that behavior while preserving navigation-during-confirmation batch cancellation. Watch registration timing has a deterministic late-file fixture, and blocked-worker shutdown has its own isolated real-pane check.

## Local evidence

- Final full Viewer integration run: **1,848 PASS**, exit 0; `Tests/bin/x64/Release/issue-121-gallery.txt`. Includes the new Browse tests and the existing themes, selection, autoplay, RAW interpretation, E/refit/manual-zoom, native keyboard, video seek/EOF, thumbnail filtering/loading/cache and GPU resource recreation paths. Earlier failures were corrected and the full suite rerun, not waived.
- Final empty-startup/RAW-default run: **131 PASS**, exit 0; `Tests/bin/x64/Release/issue-121-startup.txt`.
- Final focused Browse/navigation/interaction run: **356 PASS**, exit 0; `Tests/bin/x64/Release/issue-121-focused.txt`. Includes actual local empty/Unicode directories, drive roots and chooser, full accessible path, compaction, 100/150/200-percent caption metrics in real HWNDs, native Tab routing, exact path clipboard, Light/Dark background probes, five injected directory-error categories with partial-result rejection, silent refresh failures, superseded completion, watch-registration race, blocked-worker shutdown, live filesystem notifications and real recycle/playing-video handle release with temporary fixtures.
- CoreRegressionTests: built Release/x64/v143 and passed.
- WindowsUiTypographyTests: passed, including shared theme/command/paint/menu/frame checks and four DPI by five text-scale font cases; GDI objects remained 53 before and after.
- GalleryLayoutTests: passed for 1,000 and 1,000,000 entries.
- Instrumented and ordinary Viewer Release/x64/v143 builds passed. Binaries are under `Viewer/x64/Issue121Tests/` and `Viewer/x64/Issue121Review/`.

Reports are ignored machine-local output, not private media or tracked test artifacts. `CHANGELOG.md` contains one logical line per new bullet under Unreleased. The local version remains `0.0.0-dev`.

## Native observation and limits

Computer Use inspected the ordinary Windows app on this PC: a drive chooser labeled Drives, root D:\ with [..], bracketed root choices, and a repository sample image with the Browse directory independent of the document. The corrected header has no white bands. Actual Tab exposed the full accessible `Current folder` value and native text selection; Escape restored the compact presentation without changing the image or 100.0% zoom. Final-build follow-up observations are recorded below as completed.

The final ordinary binary was relaunched after all production changes and the repository sample was opened through File's observed first MRU item. Browse visibly displays `D:\…\fixtures` without native white bands, while the accessible value remains the exact `D:\80_sources\Q1View\Tests\fixtures\`. The source remains `sample_16x16.png`, Auto / 100.0%. This review window is left open for the user. Actual OS DPI is 96 with 100% text size.

The existing installed Store Viewer was left untouched. The standard Open dialog exposed stale structured focus information, so visible native filename-field focus was checked before literal repository-path entry; the sample opened through the ordinary user path. No private image was used as a test fixture.

No live drive was forcibly disconnected, no user folder permissions were changed, and no Windows DPI/text-size/high-contrast settings were changed. Device/permission/mid-read errors and blocked/stale completion are deterministic integration injections through the real request/publication path, not claims of testing every physical device or network provider. The new row's enlarged-font HWND checks are not actual 125/150/200-percent OS DPI acceptance. Shared high-contrast mapping is covered by common tests, but a new native OS high-contrast transition was not performed. Existing watcher registration and provider-specific cancellation behavior are not a hard real-time guarantee for every UNC provider.

The issue remains open for user review. No version bump, master merge, release tag or Store submission is part of this work.
