# Release v2.8.4: Gallery accessibility and active-file recycling

The user authorized commit, push, merge and release on 2026-10-25. This supersedes the earlier draft-only delivery restriction. Refs #122 (stages A/B) and the active-file safety portion of #127; neither issue is closed by this PR.

## Included changes

- User-approved top-left folder/drive captions with centered parent navigation and existing wrap/final-line ellipsis, card sizes, fonts and colors preserved.
- Lazy Gallery UI Automation List/ListItem providers exposing complete names, actual focus/selection and existing actions, with guarded dispatch and safe invalidation of stale items. See `issue-122-gallery-uia.md` for the full earlier UIA evidence and limitations.
- Active-file recycle safety: retain zoom/pan/rotation/ROI and paused/playing frame state on failure, including manual playback when new-file autoplay is disabled. Keep Browse focus after success; select the next survivor, then previous survivor, then a clean empty view/status.
- Release changelog and Store copy; local builds remain `0.0.0-dev`, with the release version supplied by the tagged Actions workflow.

## Local acceptance

- Focused real Viewer Browse/recycle integration: **391 PASS twice**, native process exit **0** for both runs. Generated fixtures only; no user media deleted.
- Core regression project/build and executable, GalleryLayoutTests, instrumented and ordinary Viewer Release/x64/v143 builds: passed.
- Comparator Release/x64/v143 build: passed. No Comparator product code changed in this PR.
- Earlier UIA production-code full run: **8,970 PASS**; stronger final focused UIA run: **2,602 PASS**. These are historical evidence for A/B, not a claim that those full suites were rerun after the recycle fix.
- `git diff --check`: passed. Untracked user design assets are excluded from this release.

## CI caveat and remaining issues

PR #144's earlier Gallery validation run [40164625600](https://github.com/chammoru/Q1View/actions/runs/40164625600) passed its Viewer checks but timed out in the separate Comparator appearance test; its report ends with a comparison-analysis timeout at the real LPIPS backend test. The baseline master run [39269165343](https://github.com/chammoru/Q1View/actions/runs/39269165343) already timed out in the same Comparator step before these changes. This is not represented as a fully green CI run or silently changed by extending timeouts. Build and Release was green on the earlier A/B head. The tagged release workflow remains the authority for packaging, signing, GitHub Release and Store submission; do not claim those completed before that workflow finishes.

#122 stays open for the outstanding C/live-OS validation combinations. Narrator was explicitly excluded by the user; no speech acceptance is claimed. #127 stays open because representative filenames and its confirmation/result UX design are not all implemented by this safety fix. Do not mark either whole issue complete based on this release.
