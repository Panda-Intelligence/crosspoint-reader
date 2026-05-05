# Mate targeted upload + docs sync

## Goal

Two adjacent improvements to the Mate companion app:

1. **Upload to a chosen folder** — extend the Transfer screen so users
   can pick the destination directory on the device's SD card, instead
   of always landing in `/`. Firmware already accepts
   `POST /upload?path=<dir>` (verified at
   `src/network/CrossPointWebServer.cpp:744-751`).
2. **README + ARCHITECTURE sync** — bring the two docs in `app/` up to
   date with everything that's been built (auth, scanner, discover,
   transfer, wallpaper, settings, fonts, OPDS, files, status panel).

## Scope

### A. TransferApi + transfer.tsx

`app/src/api/transfer.ts`:
- Change `uploadFile(file, onProgress)` to
  `uploadFile(file, options?)` where options is
  `{ targetPath?: string; onProgress?: (n: number) => void }`. Retain a
  backward-compat overload by treating a function as the legacy
  `onProgress`.
- Append `?path=<targetPath>` to the request URL when set; otherwise
  POST to `/upload` as today (which goes to `/`).

`app/app/transfer.tsx`:
- Add a "Destination" section above the Pick & Send button, showing
  the chosen target (default `/`) with a "Choose folder…" button.
- Tapping "Choose folder…" opens a Modal with an inline directory
  picker that uses `FilesApi.list` to navigate the tree. The picker:
  - Starts at `/`, has a breadcrumb / Up button.
  - Lists only directories.
  - "Use this folder" button confirms.
- Pass the chosen path through to `TransferApi.uploadFile`.

`app/app/wallpaper.tsx` is OUT OF SCOPE for this round — wallpaper
files don't have a logical "folder" affordance for the user.

### B. Docs

`app/README.md`:
- Replace the "Core Features" section with the actual current feature
  list (paired down to user-visible behavior, not internals).
- Add a "Screens" overview table.
- Mention the new deps that have accumulated (`expo-image-manipulator`,
  `expo-network`, `expo-sharing`).

`app/ARCHITECTURE.md`:
- Replace the "API Endpoints" section with the full list now consumed:
  `/api/status`, `/api/settings`, `/api/files`, `/download`, `/upload`,
  `/mkdir`, `/delete`, `/rename`, `/move`, `/api/fonts*`, `/api/opds*`.
- Note the form-encoded vs JSON convention per endpoint.
- Document the discovery model (QR pairing carries a token; Wi-Fi
  scan does not, so 401 is expected when interacting with auth-gated
  endpoints from a discovery-only pair).

## Acceptance Criteria

- [ ] `cd app && npx tsc --noEmit` returns exit 0.
- [ ] Transfer screen shows current destination (default `/`).
- [ ] User can navigate the tree, pick a folder, and the upload lands
      in that folder.
- [ ] README + ARCHITECTURE accurately describe the current feature set.

## Out of Scope

- Folder picker as a generic shared component (keep it inline for now;
  refactor into a shared component is a future task).
- Multi-file upload.
- Wallpaper folder targeting.
- Batch operations.

## Definition of Done

- One commit, scope-limited.
- `npx tsc --noEmit` green at the tip.

## Open Questions

None — the firmware contract is verified and the docs scope is
self-contained.
