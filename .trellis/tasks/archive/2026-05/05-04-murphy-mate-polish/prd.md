# Murphy Mate companion polish

## Goal

Polish the Murphy Mate Expo companion app (`app/`) with four concrete
follow-ups identified after the prior round (settings array-shape fix,
Device tab, modal removal). All four are user-visible improvements that
ship inside the existing `app/` Expo project — no firmware changes.

## Scope (4 items, executed sequentially with one commit per item)

### Item 1 — Wallpaper image preprocessing

`app/app/wallpaper.tsx` currently sends the raw picked image to `/upload`
unchanged. The architecture doc (`app/ARCHITECTURE.md:35`) says the app
"optionally crops/downsamples the image to fit the ESP32-S3 screen
dimensions (e.g. 480x800)". Implement that:

- Use `expo-image-manipulator` (already a transitive dep via `expo-image`?
  if not, add it; should be already in `expo` SDK). Resize the picked
  image to fit within 480×800 (preserve aspect, longest-edge cap), JPEG
  quality 0.8.
- Upload the resized version, not the original.
- Skip preprocessing entirely if the source is already ≤480×800 in both
  dimensions.
- Show the resize step in the progress UI (e.g., "Preparing image…"
  before "Uploading: X%").

### Item 2 — Real GitHub OAuth

`app/app/login.tsx:53-63` has a mock GitHub login that just stuffs a fake
user into AuthContext. Replace with a real `expo-auth-session`-based
GitHub OAuth flow:

- Use `AuthSession.useAuthRequest` against GitHub's
  `https://github.com/login/oauth/authorize` endpoint.
- Exchange the auth code for a token via the proxy / direct call (note:
  GitHub OAuth normally requires a backend to keep the secret. For now,
  use a Device-Flow or PKCE pattern that doesn't need a secret. If
  neither works, document the limitation and keep the placeholder
  client_id with a TODO.)
- On success, fetch `https://api.github.com/user` → name, login (as
  `name` fallback), email (via `/user/emails` if scope granted),
  avatar_url.
- Mirror the Google flow's storage/login pattern (call `useAuth().login`
  with the resulting `User` object).
- Keep the placeholder client IDs as constants at the top of the file
  (just like Google) with a comment that they must be replaced before
  shipping.

### Item 3 — mDNS / Bonjour auto-discovery

QR code scanning works but is friction. Add discovery so users can pick
a reachable Crosspoint Reader from a list (still allow manual QR scan
as fallback).

- Crosspoint Reader currently advertises a UDP discovery beacon
  (`src/network/CrossPointWebServer.cpp:245-246` — `LOCAL_UDP_PORT`).
  Investigate the protocol; if it's a simple broadcast, listen and
  collect responders. If it's mDNS-based (look for `MDNS.begin` in
  firmware), use a React Native mDNS lib (e.g. `react-native-zeroconf`
  if it's compatible with Expo SDK 54 + new architecture; otherwise
  document why mDNS isn't viable and fall back to UDP listen).
- Add a "Discover Devices" button on the Home screen (next to/instead
  of "Scan QR Code") that opens a modal/screen listing detected
  devices. Tap → save IP via `DeviceStorage.setIPAndToken(...)` and
  return.
- Keep QR scanning as the primary auth-token-bearing path (since the
  device QR includes a `?t=...` token that mDNS won't carry). Discovery
  should be marked as "untrusted — no token" if no token is exchanged;
  in that case the API client's auth header will be empty and any
  authed endpoint will 401 → user is told to scan QR for full access.

### Item 4 — Fonts and OPDS management UI

The firmware exposes:

- `/api/fonts` (GET), `/api/fonts/reload` (POST), `/api/fonts/delete`
  (POST) — `src/network/CrossPointWebServer.cpp:217-219`
- `/api/opds` (GET, POST), `/api/opds/delete` (POST) —
  `src/network/CrossPointWebServer.cpp:222-224`

Add two screens:

- `app/app/fonts.tsx` — list installed font packs, support delete +
  reload. Upload-new-font defers to a future round (font binary upload
  is a separate SD/SPIFFS path).
- `app/app/opds.tsx` — list configured OPDS servers, add/edit/delete
  using the same pattern as the firmware's `SettingsPage.html` OPDS
  block (`src/network/html/SettingsPage.html:484+`).

Add entry rows on the Device tab (`app/app/(tabs)/explore.tsx`) in a
new "Library" section that link to these two screens, and only show
them when `status === 'reachable'`.

Add corresponding `FontApi` and `OpdsApi` modules under
`app/src/api/`.

## Deliverable per item

1. Code changes confined to `app/`.
2. `cd app && npx tsc --noEmit` returns exit 0.
3. One git commit per item, scope-limited (`git add -- app/...`).
4. Brief commit message: `feat(app): <what shipped>`.

## Out of scope

- Wallpaper dithering for e-ink (1-bit / 4-gray) — defer to a later
  round; just resize for this task.
- Font binary upload (only list/delete/reload).
- iOS / Android native builds — the Expo dev client / EAS build is
  out of scope.
- Firmware-side changes.
- Updating CLAUDE.md, README.md, ARCHITECTURE.md — only code.

## Definition of Done

- All four items merged sequentially (4 commits on `feat/murphy`).
- `npx tsc --noEmit` green at the tip.
- No new uncommitted `app/` files at the end.

## Open Questions

- (Item 2) GitHub OAuth without a backend: PKCE + Device Flow is the
  cleanest path, but `expo-auth-session` may need a custom flow.
  Decide during implementation; if blocked, leave a clear TODO and
  keep mock behavior for that branch.
- (Item 3) Whether the firmware's UDP discovery beacon is parseable
  client-side. Confirm by reading `src/network/CrossPointWebServer.cpp`
  around the UDP setup.
