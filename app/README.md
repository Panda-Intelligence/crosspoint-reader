# Murphy Mate

Murphy Mate is the official companion application for the Crosspoint Reader (ESP32-S3 e-ink OS). Positioned similarly to the Apple Watch companion app on iOS, Murphy Mate bridges the gap between your powerful smart device and the distraction-free e-ink reader.

## 📦 Bundle Identifier
`ai.pandacat.app.murphy.mate`

## ✨ Core Features
1. **User Authentication**
   * Login via Google & GitHub.
   * securely binds your Murphy Mate instance to your personal cloud/sync account.

2. **File & E-Book Transfer**
   * Instantly beam files (EPUB, TXT, CBZ) directly to the Crosspoint Reader over local Wi-Fi.
   * Replaces the need to plug out the SD card or run local CLI scripts.

3. **Remote Settings Management**
   * Alter Crosspoint configurations remotely: fonts, sleep screen modes, Wi-Fi credentials, language settings, and reading preferences.

4. **Wallpaper / Sleep Screen Transfer**
   * Pick images from your phone gallery.
   * Send custom sleep screens or wallpapers directly to the e-ink display.

## 🏗 System Architecture

Murphy Mate is built on **React Native** & **Expo Router** and communicates with the ESP32-S3 Crosspoint Reader via local IP.

For a detailed protocol and component breakdown, see [ARCHITECTURE.md](ARCHITECTURE.md).

## 🚀 Getting Started

```bash
# Install dependencies
npm install

# Start Expo development server
npx expo start
```
