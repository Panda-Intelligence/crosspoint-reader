# Dashboard icon semantics

## Goal

Replace the WeatherClock shortcut icon with a semantically correct
weather-themed asset, and review the other 10 shortcut icons in the same
pass for consistency.

## What I already know

- Current mapping in `src/DashboardShortcutStore.cpp:30`:
  `{DashboardShortcutId::WeatherClock, ..., UIIcon::Recent}` — Recent is a
  clock icon, captures the "时间" half but not "天气".
- `UIIcon` enum in `src/components/themes/BaseTheme.h:73`:
  `{Folder, Text, Image, Book, File, Recent, Settings, Transfer, Library,
   Wifi, Hotspot}`. No weather/cloud/sun option.
- Each icon has both 32×32 (`*Icon`) and 64×64 (`*64Icon`) variants under
  `src/components/icons/`. Both must be added when introducing a new
  semantic.
- DashboardShortcutDefinition couples `DashboardShortcutId → UIIcon` at
  construction time; the dashboard renders 64×64 in iOS-grid mode and
  32×32 elsewhere.

## Assumptions (temporary)

- Adding one new `UIIcon::Weather` entry + matching 32/64-px BMP assets
  is sufficient for the v1 fix.
- Reviewing the other 10 icons is opportunistic — only re-map ones that
  are obviously wrong.
- Asset format follows the existing `*.h` static `uint8_t[]` convention,
  not external BMP files at runtime.

## Open Questions

- (Preference) Visual style for the new weather icon:
  1. Cloud + sun (mixed-weather, generic)
  2. Sun only (simpler, less crowded at 32×32)
  3. Cloud only (matches the "weather" word literally)
- (Blocking) Does the user have a preferred BMP asset to drop in, or
  should I auto-generate one (e.g. via a small Python script that
  rasterizes a vector to monochrome)?
- (Preference) Should the other 10 icons be reviewed in this same task,
  or kept out of scope?

## Requirements (evolving)

- New `UIIcon::Weather` enum value added in
  `src/components/themes/BaseTheme.h`.
- New `weather.h` (32×32) + `weather64.h` (64×64) BMP byte arrays in
  `src/components/icons/`.
- `iconBitmap64ForName` and the 32-px equivalent updated to handle the
  new value.
- `DashboardShortcutStore.cpp` `kDefinitions[WeatherClock]` switched
  from `UIIcon::Recent` → `UIIcon::Weather`.
- Build clean (`pio run -e mofei`).

## Acceptance Criteria (evolving)

- [ ] After flashing, dashboard's WeatherClock cell shows the new icon
      (not the clock icon)
- [ ] No other dashboard cells regress (visual diff)
- [ ] 32-px and 64-px variants both render cleanly (no garbled bytes)

## Definition of Done

- `pio run -e mofei` SUCCESS
- `git diff --check` exit 0
- Mofei device flash + manual visual confirmation
- New icon BMP byte data validated (correct width/height in header
  comment)

## Out of Scope (explicit)

- Animated icons
- Theme-specific icon variations
- Color icons (panel is monochrome / 4-grayscale)
- Replacing existing icon resolutions (only adding new `Weather`)

## Technical Notes

- Existing icon style: 1-bit, top-down, MSB-first, byte-packed rows.
- See `book.h` (32×32) and `book64.h` (64×64) as canonical examples.
- The dashboard cell box at iOS-grid renders the icon centered above
  label + subtitle; the new asset should leave a 2–3 px transparent
  border so it doesn't crowd the cell border.

## Research References

- (none yet)
