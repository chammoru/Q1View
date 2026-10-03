# UI screen proposals by semantic job

These are proposed Windows UI illustrations, not screenshots of an implemented Q1View build and not verification evidence. The preview uses synthetic lake/mountain imagery and illustrative numeric values; no user photographs were uploaded. Surrounding controls provide context from the roadmap; each leaf issue limits what its job may change. There is no literal before/after claim about the current native build.

The original #111–#115 issues are coordination epics. #116 remains one coherent optional layout job rather than receiving a redundant one-item wrapper. Each leaf job must be discussed, agreed, implemented and verified separately. Do not infer approval from a mockup interaction or advance automatically. Verification jobs depict their targets and add no new product UI. The multi-row layout is a separate follow-up, not an initial-release blocker.

| Job | Coordination issue | UI illustration |
| --- | --- | --- |
| 1. Unify Korean/English UI typography | [#111](https://github.com/chammoru/Q1View/issues/111) | [Typography](assets/typography.jpg) |
| 2. Add coherent appearance themes without changing image pixels | [#111](https://github.com/chammoru/Q1View/issues/111) | [Appearance](assets/themes.jpg) |
| 3. Make command availability and interaction states consistent | [#111](https://github.com/chammoru/Q1View/issues/111) | [Command states](assets/command-states.jpg) |
| 4. Refine the Viewer command and status bars | [#112](https://github.com/chammoru/Q1View/issues/112) | [Viewer shell](assets/viewer-shell.jpg) |
| 5. Make Browse parent and drive navigation visible | [#112](https://github.com/chammoru/Q1View/issues/112) | [Parent and drives](assets/path-navigation.jpg) |
| 6. Make Gallery folder names wrap before final-line ellipsis | [#112](https://github.com/chammoru/Q1View/issues/112) | [Folder names](assets/folder-gallery.jpg) |
| 7. Clarify selected files and their applicable menu actions | [#112](https://github.com/chammoru/Q1View/issues/112) | [Selection and menu](assets/selection-menu.jpg) |
| 8. Organize Viewer Info into File, Input, Display and Pixel | [#113](https://github.com/chammoru/Q1View/issues/113) | [Info panel](assets/info-panel.jpg) |
| 9. Show RAW interpretation controls only for RAW inputs | [#113](https://github.com/chammoru/Q1View/issues/113) | [RAW controls](assets/raw-controls.jpg) |
| 10. Show compact playback and frame controls for video only | [#113](https://github.com/chammoru/Q1View/issues/113) | [Playback](assets/playback.jpg) |
| 11. Make Recycle Bin confirmation and operation results safe | [#113](https://github.com/chammoru/Q1View/issues/113) | [Recycle confirmation](assets/recycle-feedback.jpg) |
| 12. Preserve context-aware shortcuts and accessible focus | [#113](https://github.com/chammoru/Q1View/issues/113) | [Keyboard and focus](assets/keyboard-focus.jpg) |
| 13. Make Comparator source roles and filenames clear | [#114](https://github.com/chammoru/Q1View/issues/114) | [Comparison sources](assets/comparator-headers.jpg) |
| 14. Present honest metric scope, values and calculation states | [#114](https://github.com/chammoru/Q1View/issues/114) | [Metric results](assets/metric-results.jpg) |
| 15. Make sequence analysis graphs contextual and readable | [#114](https://github.com/chammoru/Q1View/issues/114) | [Sequence analysis](assets/sequence-graph.jpg) |
| 16. Verify native layout, DPI, themes and accessible input | [#115](https://github.com/chammoru/Q1View/issues/115) | [Visual verification](assets/visual-verification.jpg) |
| 17. Verify image stability, performance and GPU recreation | [#115](https://github.com/chammoru/Q1View/issues/115) | [State preservation](assets/performance-verification.jpg) |
| 18. Update user documentation and assess release readiness | [#115](https://github.com/chammoru/Q1View/issues/115) | [Guide and readiness](assets/documentation-readiness.jpg) |
| 19. Add a four-source 2×2 layout with explicit 1×4 compatibility | [#116](https://github.com/chammoru/Q1View/issues/116) | [Four-source layouts](assets/multi-row-layout.jpg) |

Additional state examples: [Dark appearance](assets/themes-dark.jpg), [five selected files](assets/selection-five.jpg), [1×4 layout](assets/four-row.jpg).

## Source and rendering

`ui-jobs.html` is the editable HTML fragment for a compatible in-conversation visualization renderer, not a standalone website or a production MFC component. At very narrow conversation widths it reflows for legibility; that is not a proposed mobile app or a replacement for the native 640-DIP minimum-window/command-overflow rules in #110. Browser image capture uses the normal viewport because the available full-page/crop capture paths intermittently produced blank frames; blank outputs were replaced before publication.

The prototype's theme, selection and layout interactions are local demonstration controls. They do not operate on files or prove native rendering/shortcut/performance correctness. Production verification remains a separate gate.

Typography examples use [Pretendard](https://github.com/orioncactus/pretendard) v1.3.9. Icon placeholders use the [Lucide](https://lucide.dev/) family supplied by the preview runtime. Retain upstream notices if assets are bundled in production. No font/icon binary is introduced into the application by this documentation branch.


