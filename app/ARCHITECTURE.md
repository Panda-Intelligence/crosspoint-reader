# Architecture & Communication Protocol

This document explains how the Murphy Mate Expo app communicates with the ESP32-S3 Crosspoint Reader.

## 📡 Transport Layer
All communication happens over **HTTP (REST API)** on a local Area Network (WLAN).
1. The ESP32 Crosspoint device starts `CrossPointWebServerActivity`.
2. It hosts a captive portal or connects to a known Wi-Fi and displays a QR code with its Local IP address (e.g. `http://192.168.1.100/`).
3. Murphy Mate prompts the user to scan this QR code or auto-discovers the `.local` MDNS address.

## 🔌 API Endpoints (ESP32 WebServer)

The ESP32 exposes several endpoints natively handled by `CrossPointWebServer.cpp`. The companion app leverages these to interact with the device.

### 1. File & Book Transfer
* **Endpoint:** `POST /upload`
* **Format:** `multipart/form-data`
* **Workflow:**
   1. User picks a file via `expo-document-picker` (EPUB, TXT, CBZ).
   2. Axios POSTs the data to `http://<ESP32_IP>/upload`.
   3. ESP32 parses the multi-part body and streams the file to its SD Card.

### 2. Device Settings
* **Endpoint:** `GET /api/settings` and `POST /api/settings`
* **Format:** `application/json`
* **Workflow:**
   1. Fetch current settings (`CrossPointSettings.json` format) using `GET`.
   2. Edit settings within the app's React Native UI (e.g. changing `language`, `orientation`, `fontFamily`).
   3. `POST` the serialized JSON object back to the reader.

### 3. Wallpapers & Images
* **Endpoint:** `POST /upload`
* **Workflow:**
   1. User selects an image using `expo-image-picker`.
   2. App optionally crops/downsamples the image to fit the ESP32-S3 screen dimensions (e.g. 480x800 or 528x880) to reduce payload size.
   3. Image is POSTed to the device. Crosspoint handles format conversion (if it's not raw BMP, the internal PNGdec/JPEGDEC will process it if invoked by a specific handler, otherwise the app might convert it to `.bmp` or 1-bit `.epf` format prior to sending, depending on future SDK enhancements).

## 🔒 Authentication (Companion App)
* Users authenticate via **OAuth 2.0 (Google / GitHub)** inside the Expo App using `expo-auth-session` / `@react-native-google-signin/google-signin`.
* Tokens are securely cached locally using `expo-secure-store`.
* This profile is used to store presets locally on the phone or to pull down remote OPDS/KOReader credentials to pipe them over to the ESP32 via settings APIs.
