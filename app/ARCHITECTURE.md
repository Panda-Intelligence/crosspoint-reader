# Architecture & Communication Protocol

This document explains how the Murphy Mate Expo app talks to the
ESP32-S3 Crosspoint Reader.

## 📡 Transport

All communication is plain HTTP over the local Wi-Fi network. There is
no cloud round-trip. The Crosspoint Reader's `CrossPointWebServer`
listens on port 80 (and a binary-upload WebSocket on a configurable
secondary port — currently unused by Mate).

The companion app discovers and stores the reader's base URL via two
paths:

1. **QR pairing (recommended)** — the device displays a QR encoding
   `http://<ip>/?t=<token>`. The Scanner screen
   (`app/scanner.tsx`) parses both, persists `<ip>` as the base URL
   and `<token>` for `Authorization: Bearer <t>`.
2. **Wi-Fi discovery** — the Discover screen
   (`app/discover.tsx`) scans the local /24 subnet by HEAD/GET-probing
   `/api/settings` on every IP, with a 1.5 s timeout and concurrency
   32. Devices that respond 200 or 401 are listed. Tapping one stores
   the URL only — there is no token, so any auth-gated endpoint will
   return 401 and the user must do QR pairing for full access.

The base URL and token live in `expo-secure-store` under
`murphy_mate_device_ip` and `murphy_mate_device_token`. The `getApiClient`
helper in `src/api/client.ts` builds an axios instance with the right
`baseURL` and `Authorization` header on every call.

## 🔌 Firmware endpoints consumed

All endpoints live on the device's HTTP server. Routes registered at
`src/network/CrossPointWebServer.cpp:189-225`.

### Status

- `GET /api/status` — `{ version, ip, mode ("AP"|"STA"), rssi, freeHeap, uptime }`.
  Wrapped by `src/api/status.ts:StatusApi.get()`.

### Settings

- `GET /api/settings` — array of metadata objects, one per
  user-visible setting:
  ```jsonc
  [
    { "key": "fontSize",  "name": "Font Size", "category": "Display",
      "type": "enum",  "value": 2,
      "options": ["Tiny", "Small", "Medium", "Large", "X-Large"] },
    { "key": "screenMargin", "type": "value", "value": 5,
      "min": 0, "max": 20, "step": 1, ... },
    { "key": "deviceName", "type": "string", "value": "Murphy" },
    { "key": "wifiAutoconnect", "type": "toggle", "value": 1 }
  ]
  ```
- `POST /api/settings` — flat JSON diff `{ key: value, ... }`. Only
  the included keys are applied; the firmware ignores out-of-range
  values silently.

Wrapped by `src/api/device.ts:DeviceApi`. The Settings screen
(`app/settings.tsx`) groups by category and dispatches per `type`.

### Files (read)

- `GET /api/files?path=<dir>` — returns `[{ name, size, isDirectory, isEpub }, ...]`.
  Hidden items (e.g. `.crosspoint`, `.mofei`) are filtered server-side
  unless `showHiddenFiles` is true.
- `GET /download?path=<file>` — binary attachment. The app stores the
  result under `Paths.document/crosspoint/<basename>` then opens the
  system Share sheet via `expo-sharing`.

### Files (mutation) — **all form-encoded**, NOT JSON

These handlers read with `server->arg("name")` so the bodies are
`application/x-www-form-urlencoded`. The app uses `URLSearchParams` so
axios picks the right Content-Type automatically.

- `POST /upload?path=<dir>` (multipart) — file goes into `<dir>`
  (defaults to `/`). Path is read from the query string at upload START
  because multipart form fields aren't available until after the body
  is parsed.
- `POST /mkdir`           — `path` (parent), `name` (new folder).
- `POST /delete`          — `path` (single) or `paths` (JSON array of
  paths). Mate uses the single-path form.
- `POST /rename`          — `path` (existing), `name` (new basename;
  no slashes; protected names rejected).
- `POST /move`            — `path` (existing), `dest` (full target path
  including new basename).

### Library management

- `GET /api/fonts` → `{ packs: [{ name, path, installed, active }] }`.
- `POST /api/fonts/reload` — re-scan `/.mofei/fonts/` on the SD card.
- `POST /api/fonts/delete` — JSON `{ path }` (this one IS JSON, unlike
  the file-management endpoints above).

- `GET /api/opds` → `[{ index, name, url, username, hasPassword }]`.
  **Passwords are never returned** — only `hasPassword`.
- `POST /api/opds` — JSON `{ name, url, username, password?, index? }`.
  Omit `password` to preserve the existing one; pass an empty string to
  clear; pass `index` to update an existing entry.
- `POST /api/opds/delete` — JSON `{ index }`.

## 🔒 App-level authentication

Sign-in is independent from device pairing. Signed-in identity is used
to scope cloud presets and (eventually) to pull credentials such as
KOReader sync.

- **Google** — `expo-auth-session/providers/google` in the implicit
  flow. Token exchanged for the Google profile via
  `https://www.googleapis.com/userinfo/v2/me`.
- **GitHub** — `expo-auth-session` authorization-code + PKCE. No
  client_secret bundled. Exchange happens at
  `github.com/login/oauth/access_token` with the PKCE verifier. The
  app tries `/user`, then falls back to `/user/emails` if the primary
  email isn't returned by `/user`.

The signed-in `User` is persisted in `expo-secure-store` under
`murphy_mate_user`.

## 🔐 Security notes

- The device token (`?t=<...>` from the QR) is sent as
  `Authorization: Bearer <token>`. It exists alongside any device-side
  password / passcode the user has set.
- The lock-screen passcode (firmware feature) is a separate gate that
  only affects wake-from-sleep on the device itself; it is **not** an
  app-level credential.
- OAuth client_id placeholders must be replaced before production.
  Google is read-only profile fetch; GitHub uses scopes
  `read:user user:email`.

## 🚦 Discovery semantics

A device discovered via Wi-Fi scan but never QR-paired is "trusted
half" — the IP is known but no token is stored. Mate intentionally
does NOT mask this: the Device tab will show "Reachable" but
auth-gated endpoints will surface 401 errors. The recommended flow is
QR scan; Discover-on-Wi-Fi exists as a fallback when the QR is
inaccessible.
