# Reading Interface Traditional Chinese Wishlist Slice

## Goal

Implement a pragmatic first slice of the user's Traditional Chinese reading-interface wishlist in the current firmware Reader, while preserving already implemented reader behavior and keeping unrelated Mate app work untouched.

## What I Already Know

* User scope is Reader/EPUB reading experience only: `src/activities/reader`, `lib/Epub`, and shared reader utilities/stores as needed.
* User explicitly said non-reading wishlist items should be written into todo/future planning instead of implemented now.
* Current dirty work from other streams must not be reverted or overwritten, especially Mate app docs/files and `.trellis/tasks/05-05-mate-targeted-upload-and-docs/`.
* Large unsupported firmware libraries are risky; hard features such as full Simplified-to-Traditional conversion, vertical layout, and dictionary integration should be deferred unless a safe existing pattern exists.

## Requirements

* Audit existing Reader/EPUB code before editing and preserve current support for recent reading, EPUB images, bookmarks, page numbers, font size controls, and touch lock.
* Implement bounded Reader UX improvements where the current architecture supports them safely.
* Prefer existing UI primitives, storage patterns, and rendering helpers.
* Do not modify `app/` Mate files.
* Add a repo future-plan todo document for non-reading wishlist items and hard Reader items that cannot be safely completed in this slice.
* Keep firmware build compatibility as the acceptance bar.

## Acceptance Criteria

* [ ] Reader changes compile for the Mofei firmware target.
* [ ] Existing Reader features are not removed.
* [ ] At least one concrete Reader UX improvement from the missing/partial wishlist is implemented.
* [ ] Deferred non-reading wishlist items are tracked in a repo todo/future-plan document.
* [ ] Verification commands and blockers are recorded in the final handoff.

## Out of Scope

* Mate app file transfer/docs work.
* Full Simplified-to-Traditional language conversion engine unless a safe existing dependency already exists.
* Full vertical CJK layout engine.
* Full offline dictionary engine.
* Broad app platform/navigation refactors.

## Technical Notes

* Trellis shared guides read before implementation:
  * `.trellis/spec/guides/index.md`
  * `.trellis/spec/guides/code-reuse-thinking-guide.md`
  * `.trellis/spec/guides/cross-layer-thinking-guide.md`
* Available package spec indexes are mostly placeholders; relevant concrete rules include Mofei input/quality rules in `.trellis/spec/open-x4-sdk/frontend/quality-guidelines.md` and `.trellis/spec/open-x4-sdk/backend/quality-guidelines.md`.
