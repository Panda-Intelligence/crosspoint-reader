# Murphy Reader — Changelog

## murphy-26-0505-1.0.0 (2026-05-05)

### Added
- Web USB flasher via ESP Web Tools (Chrome/Edge)
- OTA update API (`/ota/latest`, `/ota/check`)
- Cloudflare Worker serving firmware from R2
- Touch offset calibration params (`MOFEI_TOUCH_OFFSET_X/Y`)
- Six advanced Reader features:
  - Simplified-to-Traditional text conversion
  - Vertical CJK layout mode
  - Text-range highlight selection + persistence
  - Footnote body popup overlay
  - Full-book search
  - In-reader dictionary lookup
- Mate companion app: OAuth env config, file download fix, API error toast with 401 redirect, theme constants, a11y labels
- Bottom button hints hidden (touch screen only)
- Mofei QEMU simulator peripherals (FT6336U touch, GDEQ0426T82 display)

### Changed
- EPUB Section cache version bumped to 23
- Keyboard entry uses touch tap for character input
- ActivityResult flow enhanced for reader sub-activities

### Fixed
- File re-download pre-delete in FilesApi
- 401 errors now redirect to QR pairing instead of generic toast
- `pio check` defects resolved
