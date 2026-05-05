# Mate ebook library

## Goal

Surface the device's `RecentBooksStore` to the Murphy Mate companion
app so the user can see what they've been reading at a glance, with
covers, and tap through to the existing file actions
(download / share / delete).

## What I already know

- Firmware-side store exists:
  `src/RecentBooksStore.h` — singleton `RECENT_BOOKS`, vector of
  `{path, title, author, coverBmpPath}`, capped at 10 entries, persisted
  via `JsonSettingsIO`.
- Cover BMPs are stored under `.crosspoint/epub_<hash>/cover.bmp`. They
  can be served by the existing `GET /download?path=<bmp>` handler —
  no new firmware endpoint needed for image bytes.
- The firmware does NOT currently expose `/api/books/recent` over HTTP.
  We will add it.
- HTTP server route table: `src/network/CrossPointWebServer.cpp:189-225`.
  The new handler will live alongside `handleGetSettings` /
  `handleGetFontPacks` etc.
- Authoritative cover thumbnail format is 1-bit BMP, ~104×156 px (this
  is the format `ImageView` consumes on-device); the app can render
  these as `<Image source={{ uri }}/>` after downloading via
  `expo-file-system` since RN's image loader handles BMP on iOS but
  not Android. We will accept that platform skew for v1 and fall back
  to a placeholder when load fails.

## Scope

### A. Firmware

- New handler: `handleGetRecentBooks() const` returning the recent-books
  array as JSON:
  ```jsonc
  [
    { "path": "/Library/foo.epub", "title": "Foo", "author": "Bar",
      "coverPath": "/.crosspoint/epub_abc/cover.bmp" }
  ]
  ```
  (Note: rename `coverBmpPath` to `coverPath` in the API for forward-
  compat — drop the format hint.)
- Register `GET /api/books/recent` next to other read-only `/api/*`
  routes.
- Auth-gate identical to other `/api/*` endpoints (`checkAuth`).

### B. App

- `app/src/api/books.ts` — `BooksApi.recent()` returning the typed
  array. Provide a helper `coverImageUrl(coverPath)` that joins the
  base URL + `/download?path=<coverPath>&t=<token>` so the resulting
  URL works as an `<Image source>` directly.
- `app/app/library.tsx` — library screen:
  - Top: title "Recent Books".
  - Grid of cover cards (2 columns), each with cover image, title,
    author. Empty state if list is empty.
  - Tap a card → existing per-file action sheet (download / rename /
    move / delete) on the book's path. Reuse the patterns from
    `files.tsx` if convenient.
- Device tab: replace the "Files" Library entry with two rows:
  "Recent Books" (→ `/library`) and "All Files" (→ `/files`).

### C. Build verification

- `pio run -e mofei` clean.
- `cd app && npx tsc --noEmit` clean.

## Acceptance Criteria

- [ ] `GET /api/books/recent` returns the current store contents as
      JSON.
- [ ] App's library screen renders recent books with covers (or
      placeholders when image load fails).
- [ ] Tapping a book opens the action sheet with download / delete.
- [ ] Empty state shows a clear "Read a book on your device to see it
      here" message.

## Out of Scope

- Reading progress / page-position sync.
- Full-text search across books.
- Batch operations on books.
- Covers at higher resolution than what's already on disk.
- Adding new books from the app (the existing Transfer flow handles
  that already; targeted upload too).
- Server-side metadata enrichment (title/author come from the EPUB
  parser inside the firmware; nothing to do here).

## Definition of Done

- One firmware commit + one app commit (or one combined commit if
  same scope), both behind the new `/api/books/recent` contract.
- pio + tsc green.
- Task archived.
