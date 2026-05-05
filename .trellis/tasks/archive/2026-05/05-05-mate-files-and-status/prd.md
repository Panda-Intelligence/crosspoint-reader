# Mate file browser and status

## Goal

Add two cross-cutting features to the Murphy Mate Expo app, both
consuming firmware endpoints that already exist:

1. **SD file browser** — browse directories, navigate, download books
   back to the phone, delete, mkdir.
2. **Live device status** — battery / firmware version / WiFi RSSI /
   free heap / uptime surfaced on the Device tab.

## What I already know

Firmware endpoints (verified in
`src/network/CrossPointWebServer.cpp:189-225`):

- `GET  /api/status`              → `{version, ip, mode, rssi, freeHeap, uptime}`
- `GET  /api/files?path=<dir>`    → `[{name, size, isDirectory, isEpub}, ...]`
- `GET  /download?path=<file>`    → binary; `Content-Disposition: attachment`
- `POST /upload`                  → multipart (already wired)
- `POST /mkdir`                   → `{path}`
- `POST /rename`                  → `{path, newName}` (assumed; will verify)
- `POST /move`                    → `{path, newPath}` (assumed; will verify)
- `POST /delete`                  → `{path}`

Hidden-item filtering is enforced server-side by the `showHiddenFiles`
setting; the API does not expose `.crosspoint`, `.mofei`, etc. unless
that setting is on.

## Scope

### A. New API modules

- `app/src/api/files.ts` — `FilesApi` with `list`, `download`,
  `remove`, `mkdir`, `rename`, `move`. Download path saves to the app's
  document directory via `expo-file-system` and surfaces a "Saved as
  …" alert with a Share intent. We will NOT enumerate
  rename / move semantics until the firmware POST shapes are verified.
- `app/src/api/status.ts` — `StatusApi.get()` returning
  `{version, ip, mode, rssi, freeHeap, uptime}`.

### B. New file browser screen

- `app/app/files.tsx` — a directory-listing screen.
- Navigation: tapping a directory pushes its path. A breadcrumb at the
  top shows the current path; back button pops one level (or exits the
  screen at root).
- Per row: name, size (formatted), type icon (folder / book / file).
- Long-press → row action sheet: Download, Delete (with confirmation),
  Rename (deferred if shape unclear).
- "New folder" button at the bottom → modal text input → POST `/mkdir`.
- Empty state, loading state, error state with retry.

### C. Device tab status panel

- `app/app/(tabs)/explore.tsx` — replace the read-only "Reachable /
  Unreachable" pill with a richer status panel: firmware version, IP
  + mode, RSSI bars, free heap (KB), uptime (h:m:s).
- Refresh on tab focus + manual refresh button.
- Add a "Files" link to the Library section that pushes `/files`.

## Acceptance Criteria

- [ ] `cd app && npx tsc --noEmit` returns exit 0.
- [ ] App can browse `/`, navigate into a sub-directory, navigate back.
- [ ] App can download a small file and confirm via toast/alert.
- [ ] App can delete an empty directory and a regular file.
- [ ] App can create a new folder under an existing directory.
- [ ] Device tab shows real status when paired and reachable; falls
      back to "Reachable / Unreachable" on permission/connectivity
      errors.
- [ ] No regression on existing screens (settings, scanner, transfer,
      wallpaper, fonts, opds, login).

## Out of Scope

- Rename / move (deferred — requires firmware-side confirmation of
  request shape before app implements).
- Multi-select operations (select N files then delete).
- ZIP archive / batch download.
- Streaming preview of EPUB / TXT in-app (just download).
- WebSocket-based fast upload (separate future task).

## Definition of Done

- Two items above merged in one or two commits, each scope-limited.
- `npx tsc --noEmit` green at the tip.
- README/ARCHITECTURE updates deferred until a separate doc round.

## Dependencies

- May need `expo-file-system` (verify presence; install via
  `npx expo install` if missing).
- May need `expo-sharing` for the post-download Share intent.

## Open Questions

- POST shapes for `/rename` and `/move` — confirm by reading the
  firmware handlers before implementing those operations.
