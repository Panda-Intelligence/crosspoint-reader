# Mate file browser polish

## Goal

Three small additions that round out the file browser shipped in
05-05-mate-files-and-status:

1. **Rename** — long-press a file/folder → rename via modal.
2. **Move** — long-press → move into another folder picked from the
   current device tree.
3. **Post-download Share** — after download, present the iOS/Android
   share sheet so the user can hand off the file to another app.

## What I already know

Firmware-side handlers already accept these operations
(`src/network/CrossPointWebServer.cpp`):

- `POST /rename`   → form-encoded `path` + `name`
  (the new basename — no slashes; protected names rejected)
- `POST /move`     → form-encoded `path` + `dest`
  (full destination path; `dest` parent must exist)

Sharing is provided by `expo-sharing@~14.0.8`
(`Sharing.shareAsync(localUri)` after `isAvailableAsync()`).

## Scope

### A. Files API additions

`app/src/api/files.ts`:
- `rename(path: string, newName: string)` → POST /rename
- `move(path: string, destDir: string)` → POST /move with
  `dest = joinPath(destDir, basename(path))`

Both use `application/x-www-form-urlencoded` like the existing
`mkdir` / `remove`.

### B. Files screen UX

`app/app/files.tsx`:
- Tap on a file/folder → existing inline action sheet, plus two new
  rows: Rename, Move.
- Rename → reuse the same modal pattern as `+ New Folder`, prefilled
  with the current basename.
- Move → push a sub-screen (or use a modal listing) that lets the user
  navigate the tree from `/` and tap a destination folder. Confirm via
  a "Move here" button.

### C. Share intent after download

`app/app/files.tsx`:
- After `FilesApi.download()` succeeds, call `Sharing.shareAsync(uri)`
  if `Sharing.isAvailableAsync()` returns true. Otherwise fall back to
  the existing "Saved to …" alert.

## Acceptance Criteria

- [ ] `cd app && npx tsc --noEmit` returns exit 0.
- [ ] Rename works on both files and folders.
- [ ] Move works (target folder picker shows live remote tree).
- [ ] Download triggers the share sheet on iOS/Android; falls back
      gracefully if sharing is unavailable.
- [ ] No regression: existing list, navigate, delete, mkdir, download.

## Out of Scope

- Multi-select (batch operations).
- Cross-protocol move (between WebDAV and local).
- Drag-and-drop reordering.
- WebSocket-based fast upload (separate future task).

## Definition of Done

- One commit, scope-limited.
- `npx tsc --noEmit` green at the tip.

## Open Questions

- Move modal vs full sub-screen for picking a destination folder.
  Lean toward sub-screen (better navigation UX with breadcrumbs and
  large lists).
