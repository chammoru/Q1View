# Issues #122 C and #127: remaining Gallery coverage and safe recycle feedback

## Scope and delivery boundary — 2026-10-25

The user requested the remaining verification/design work for #122 and #127, and explicitly allowed temporary Windows display scale, text size and high-contrast changes with restoration. Narrator remains excluded at the user's request. Work is on `codex/issues-122-127-validation-recycle`, based on `origin/master` `e7e737048f867c99735039e19a5468a7515c07bf`. These are local changes awaiting review, not a new release or a claim that they are already on master. Local version stays `0.0.0-dev`.

The earlier A/B implementation and active-file recycle safety shipped in v2.8.4. This work does not redesign those features, change selection eligibility, implement #123, migrate the entire app's DPI-awareness policy, or modify Comparator/Qt product code. Unrelated untracked design assets are preserved.

## #122 C: expanded automated caption coverage

The existing DirectWrite matrix is retained: DPI inputs 96/120/144/192, text-scale inputs 1/1.5/2, narrow/wide drawer widths 210/360, all five column counts, and seven caption families covering short names, Korean, English with spaces, unbroken Latin/digits, mixed scripts, emoji/combining characters and literal brackets/ampersands. It checks the actual layout's first-line origin, whole visible rows, final-visible-line trimming and unchanged source strings/shared format.

The GDI fallback matrix now uses the corresponding actual square card text bounds instead of a single fixed 220-pixel width. It adds both Light/Dark palette backgrounds to the same DPI/text/drawer/column/name/parent matrix. Raster bounds verify top-left ordinary captions, centered parent captions, complete-height rows, no ink outside the text bounds, and no drawing when even one complete row cannot fit. An unbroken-name check requires wrapping onto additional rows when space is available, rather than early single-line ellipsis. No production caption renderer, font, card sizing or palette change was necessary.

## #122 C: actual Windows observations, distinguished from parameter tests

The interactive acceptance mode is compiled only into `Q1VIEW_GALLERY_TESTS` builds. It uses isolated `Q1ViewGalleryTests` preferences and repository-owned fixture copies under ignored test output. A timer records the native window DPI, Windows text scale, high-contrast state, application theme and actual folder-font height. It also checks the source path/open generation, stable RGB-buffer pointer and sampled bytes, and selected media paths against the initial snapshot. Missing log rows are not counted as passing; rows are written only after the actual font matches the expected current settings.

| Native setting | Observed evidence | Report under `Tests/bin/x64/Release/` |
| --- | --- | --- |
| Display 100%, text 100% | DPI 96, folder font -13 | `issue122-live-os.txt` |
| Text 150%, then 200%, then restored 100% | Native text factors 1.50/2.00/1.00, font heights -20/-26/-13, unchanged source/selection | `issue122-live-os.txt` |
| Display 125%, text 100% | Relaunched Viewer reports DPI 120, font -16 | `issue122-live-os125.txt` |
| Display 150%, text 100% | Relaunched Viewer reports DPI 144, font -20 | `issue122-live-os150.txt` |
| Display 175%, text 100% | Relaunched Viewer reports DPI 168, font -23 | `issue122-live-os175.txt` |
| Contrast None → Aquatic → None | Native high contrast 0 → 1 → 0, unchanged source/selection | `issue122-live-hc.txt` |
| App System → Dark → Light → System | Actual Gallery colors and complete accessible names; unchanged source/selection | `issue122-live-hc.txt` |

The monitor's standard Windows scale selector offered only 100/125/150/175 percent at its unchanged 1920×1080 resolution. Native 200-percent OS scaling was therefore **not** performed. Custom scaling/sign-out was not forced. DPI 192/200-percent coverage is the automated metrics/raster matrix, not a native OS claim. The Viewer manifest remains system-DPI-aware, so each non-100-percent native DPI check deliberately relaunched Viewer; this is not evidence of a new live PerMonitorV2 implementation. Text-size and contrast transitions were tested in the same running instance.

Computer Use inspected the real Gallery at narrow and wider drawer widths, enlarged text, Light/Dark and Aquatic high contrast. Ordinary captions remained top-left with wrapping; `[..]` remained centered. The actual accessibility tree retained complete Korean/English/literal/emoji/combining names separately from visual trimming and the Current folder header. The image remained the repository's 16×16 sample. Narrator was never started. These native observations do not claim that every cross-product combination was manually tested.

Windows display scale and text size were restored to their original **100%**, contrast was restored to **None/off**, and the isolated app theme returned to **System**. The original resolution and user application preferences were not changed.

## #127: implemented confirmation and result design

Use the native Windows TaskDialog, consistent with the OS-owned-dialog boundary of #110. The confirmation shows the exact singular/plural count and the first three complete Unicode basenames. Additional selections are explicitly counted. `Show details` reveals up to ten original full paths; a larger batch explicitly states that the path list is bounded. This prevents a large selection from producing an unbounded dialog while not implying that the visible preview is the entire target set.

The neutral action is explicitly **Move to Recycle Bin**, with native **Cancel** as the initial/default action. There is no red destructive styling, warning icon, invented progress or Undo button. Escape/cancellation is not consent. If TaskDialog creation fails, a native OK/Cancel fallback still defaults to Cancel. Closing a dialog restores the prior valid focus only while the owning Viewer is still foreground.

The existing captured eligible media paths, generation/folder revalidation and Shell batch remain the authority for actual targets. No directory batching, permanent-delete fallback or new selection rule is added. The confirmation callback now receives those exact paths rather than a count alone. Existing active-image/video/RAW suspend/resume, failed selection preservation, next/previous survivor and empty-view behavior are retained.

Failure feedback counts actual per-item Shell `recycled` outcomes. It names up to three failed basenames and provides up to ten failed full paths with available localized Windows failure reasons, disclosing any remainder. A successful HRESULT without a confirmed recycled item is not described as success. Unknown/unconfirmed errors use an explicit neutral statement, not fabricated diagnosis. Successful files are not included in the failure list, and no unsupported undo/permanent-deletion claim is made. Complete success continues through the existing Browse refresh instead of adding another success modal.

## Native UI review and corrections

The real four-item confirmation was inspected at 200-percent Windows text size, then cancelled; all four generated files remained intact. This caught and corrected the singular `And 1 more item.` wording and the initially reversed expand/collapse labels. The final build's real one-item confirmation was inspected under Aquatic high contrast: native Cancel had the initial focus indicator, `Show details` changed to `Hide details`, full expanded path text was exposed through UIA, and clicking Cancel restored the actual Gallery item focus without removing the file. Native visual path shortening does not replace the full accessible ExpandedInformation string.

TaskDialog buttons are DirectUI accessibility elements, not child HWND controls addressable through invented GetDlgItem IDs. Automated tests assert the real TaskDialog configuration and native cancellation return; live UI observation checks the visible default focus, labels and accessible expanded text. A screenshot-helper timeout during the high-contrast transition was recovered by refreshing the returned window selection; the native log independently recorded high contrast enabled and then restored.

## Final local acceptance

The final actual Viewer full run from an empty document passed **15,478 assertions, native exit 0**, ending with `ALL INTEGRATION CHECKS PASSED` in `Tests/bin/x64/Release/issues122-127-full.txt`. This run includes the final dialog label/wording corrections, expanded caption matrix and real native cancellation dialogs for 1/2/4/5/25-item previews. Historical A/B counts are not substituted for this run. These are assertion counts, not thousands of independent user journeys.

The full mode retains real UIA MTA-client, list/grid, preview eligibility/failure, selection/compare/recycle, directory/watcher/navigation, keyboard/E, source/zoom/ROI/playback, bounded-cache and forced fallback/GPU-recreation regressions. Actual Shell batch tests include partial lock failures, rejection of permanent-delete fallback, active-file image/view-state preservation, paused/playing video recovery with autoplay disabled, RAW frame/interpretation preservation, next/previous survivor choice, empty status and Browse focus. Only generated fixture files were recycled by automatic tests. The focused modes remain `Q1VIEW_FOLDER_CAPTION_ONLY` and `Q1VIEW_BROWSER_NAVIGATION_ONLY`, with reports selected by `Q1VIEW_GALLERY_TEST_REPORT`.

CoreRegressionTests, GalleryLayoutTests and WindowsUiTypographyTests passed in this worktree. The layout suite retained bounded work at 1,000 and 1,000,000 entries; common UI GDI objects remained 55 before/55 after. Ordinary/instrumented Viewer Release x64 v143 builds and Comparator Release x64 v143 build passed. No tagged release, packaging/signing/Store acceptance or all-green CI claim is made. The earlier unrelated Comparator LPIPS CI timeout is documented in `release-2.8.4.md` and is not masked by changing its timeouts here.

Final build logs are `Tests/bin/x64/Release/issues122-127-build.log`, `issues122-127-normal-build.log` and `issues122-127-comparator-build.log`. The reused instrumented output/intermediate directory retains the known MSB8028 shared-directory warning; the standard ordinary Viewer build is separate and does not emit that warning. `git diff --check` passed. Both issues remain open pending delivery/review of this local branch; no merge/release approval is inferred from the OS-setting permission.

## Subsequent delivery and release approval — 2026-10-25

After the local hand-off, the user explicitly requested commit, push, merge and a new release, and asked whether the issues can now be cleared. That approval supersedes the earlier local-only delivery boundary above. This change prepares **v2.8.5** and closes #122/#127 on merge for the implemented scope, with the documented Narrator exclusion and unavailable native 200-percent display setting retained rather than represented as passed. #123 and unrelated roadmap issues are not closed by this change.

The release changelog and Store description now match the implemented confirmation/result UX. Local binaries remain `0.0.0-dev`; only the tagged release workflow supplies `2.8.5`. The completed 15,478-assertion actual Viewer run remains the source acceptance evidence, followed by local core/build verification for release preparation. Packaging, signing, GitHub Release assets and Store submission are determined by the tagged Actions workflow; triggering it is not a claim that those downstream steps have completed, and no repeated completion polling is required.
