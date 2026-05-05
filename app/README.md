# Murphy Mate

Murphy Mate is the official companion app for the **Crosspoint Reader**
(ESP32-S3 Mofei e-ink panel). Positioned similarly to the Apple Watch
companion app on iOS, Murphy Mate is the bridge between your phone and
your distraction-free e-ink reader on the local network.

## 📦 Bundle Identifier
`ai.pandacat.app.murphy.mate`

## ✨ What you can do today

| Area | Feature |
|---|---|
| Pairing | Scan device QR (carries auth token), or **Discover on Wi-Fi** by scanning the local /24 subnet (no token; auth-gated endpoints will return 401) |
| Auth | Sign in with Google or GitHub (PKCE — no client secret bundled) |
| Library | **Browse** the SD card, **download** any file to your phone with a Share intent, **rename**, **move** between folders, **delete**, create new folders |
| Transfer | Pick an EPUB / TXT / CBZ and send it; **choose which folder** on the device it lands in; live progress bar |
| Wallpaper | Pick an image, get auto-resized to fit the 480×800 panel (JPEG q=0.8), then sent to the device |
| Settings | Read the firmware's full settings array dynamically and edit any toggle / enum / value / string field; sync only the diff back to the device |
| Fonts | List installed font packs, mark active vs missing, delete packs, reload from `/.mofei/fonts/` on the SD card |
| OPDS | Add / edit / delete OPDS catalog servers (preserves password when not changed) |
| Device tab | Live status panel (firmware version, AP/STA mode, RSSI, free heap, uptime), connection check, settings summary, and Library shortcuts |

## 🏗 System Architecture

Murphy Mate is built on **React Native** (Expo SDK 54) and **Expo
Router** and communicates with the Crosspoint Reader over plain HTTP on
the local Wi-Fi. See [ARCHITECTURE.md](ARCHITECTURE.md) for the
endpoint catalog, request shapes, and pairing model.

### Notable dependencies
- `expo-router` — file-based navigation
- `expo-camera` — QR scanner
- `expo-document-picker` — file picker for transfer
- `expo-image-picker` + `expo-image-manipulator` — wallpaper picker + resize
- `expo-file-system` — local storage of downloaded device files
- `expo-sharing` — system Share sheet after download
- `expo-network` — local IP for Wi-Fi /24 discovery
- `expo-auth-session` — Google + GitHub OAuth
- `expo-secure-store` — pairing token + signed-in user persistence
- `axios` — HTTP client

## 🚀 Getting Started

```bash
# Install dependencies
npm install

# Start Expo dev server
npx expo start
```

### OAuth configuration

Both `app/app/login.tsx` (GitHub) and the Google useAuthRequest call
contain placeholder client IDs (`YOUR_..._CLIENT_ID_HERE`). Replace
them with values from the GitHub OAuth App / Google Cloud Console
console **before shipping a build**. The redirect URI used by GitHub
PKCE is `murphymate://auth/github`; configure that in the GitHub
OAuth App settings.

## 📁 Repo layout

```
app/                           # Expo project
├── app/                       # Expo Router file-based routes
│   ├── (tabs)/                # Bottom tab pages: Home + Device
│   ├── login.tsx
│   ├── scanner.tsx            # QR pairing
│   ├── discover.tsx           # Wi-Fi /24 subnet scanner
│   ├── transfer.tsx           # Pick + send file (with folder picker)
│   ├── wallpaper.tsx          # Pick + send wallpaper
│   ├── settings.tsx           # Dynamic settings editor
│   ├── files.tsx              # SD file browser
│   ├── move-picker.tsx        # Destination folder picker for Move
│   ├── fonts.tsx              # Font pack manager
│   └── opds.tsx               # OPDS server manager
├── src/
│   ├── api/                   # Typed wrappers around firmware endpoints
│   │   ├── client.ts          # axios factory + auth header
│   │   ├── device.ts          # /api/settings (array-shaped)
│   │   ├── status.ts          # /api/status
│   │   ├── files.ts           # /api/files /download /mkdir /delete /rename /move
│   │   ├── transfer.ts        # /upload (with optional ?path=)
│   │   ├── fonts.ts           # /api/fonts*
│   │   ├── opds.ts            # /api/opds*
│   │   └── discover.ts        # local /24 HTTP probe
│   ├── contexts/              # AuthContext (signed-in user)
│   └── utils/                 # DeviceStorage (paired IP + token)
└── components/                # Themed primitives + tab widgets
```
