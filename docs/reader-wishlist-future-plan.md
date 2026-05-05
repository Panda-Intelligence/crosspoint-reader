# Reader Wishlist Future Plan

Date: 2026-05-05
Executor: Codex

This file tracks the Traditional Chinese wishlist items that are not safe to complete in the current Reader slice.

## Shipped Or Preserved In Current Reader

- Recent reading list: preserved; the home/recent reading list already stores recent EPUB paths and shows cover/progress when metadata is available.
- EPUB images: preserved; Reader section rendering already passes the image rendering setting through EPUB pagination/rendering.
- Bookmarks and page numbers: preserved; EPUB bookmarks remain stored per book and the Reader menu keeps chapter page and book progress summaries.
- Font size controls: preserved; Reader menu font-size up/down still reflows at the current reading position.
- Touch lock: preserved; Reader menu still toggles touch lock.
- 2-bit grayscale AA: preserved; Reader text/image render path already runs the grayscale pass when `SETTINGS.textAntiAliasing` is enabled.
- Highlight/line marking: first slice shipped as persistent page-level line marks stored next to EPUB bookmarks.
- Search limitation visibility: first slice shipped by labeling Reader search results as current-chapter scoped.
- Footnote reader-local behavior: first slice shipped by showing each current-page footnote's label and target href before opening it.
- Simplified-to-Traditional conversion: first slice shipped as a Reader setting that converts EPUB text before section pagination and cache serialization. Coverage is compact and not OpenCC-complete.

## Deferred Reader Work

- Full OpenCC-grade Simplified-to-Traditional coverage: defer until a larger table or build-time transform can be evaluated for firmware memory and cache impact. Do not add a large runtime library blindly.
- Full vertical CJK layout: defer until the page model supports vertical line layout, punctuation rotation, touch hit-testing, and pagination cache separation.
- Text-range highlights: current implementation marks a whole Reader page. True range highlights need stable text offsets, selection gestures, and serialized range anchors.
- Footnote popup with extracted note body: current implementation opens the footnote target. A popup requires resolving the target section, loading/extracting target text safely, and keeping a return stack.
- Book-wide search: current implementation searches only the loaded chapter. Full-book search needs incremental indexing, cancellation, and cache limits.
- English dictionary expansion: current dictionary remains a separate Reader hub feature. Full in-reader lookup needs selection/tokenization first.

## Non-Reading Wishlist Parking Lot

- Mate app transfer, sync, and file-management wishlist items belong to app/Mate tasks, not this Reader slice.
- Dashboard, study, arcade, desktop widgets, and system settings wishlist items should be split into dedicated Trellis tasks before implementation.
- Any web companion or import workflow changes must avoid being mixed into Reader firmware changes.

## Notes

- Keep future Reader tasks scoped around one data flow at a time: pagination/rendering, persistent state, input/selection, or companion import.
- Before adding external libraries, check firmware flash, PSRAM, and cache-file impact with `pio run -e mofei`.
