# Mate review fixes

## Goal

Fix concrete issues found in the self-review of Murphy Mate companion
app (and one supporting firmware endpoint), without adding net-new
features. Scope strictly app-side + minimal firmware additions.

## Issues to fix (from review)

### High

- **H1** OAuth client_id placeholders are source-file constants. Risk
  of accidental commit of working IDs and breakage when missing.
  → Move to env via `app.config.ts` + `expo-constants`. When unset,
    show a clear "OAuth not configured" disabled state instead of a
    cryptic 400.
- **H2** *(skip — needs hardware verification, not a code bug.)*
- **H3** `FilesApi.download()` no longer pre-deletes the target before
  `downloadAsync()`. Re-downloading the same file may fail. Add an
  explicit `deleteAsync(uri, { idempotent: true })` first.

### Medium

- **M1** Folder Action Sheet on a non-empty directory: surface clear
  feedback when firmware rejects deletion. Document the limitation in
  the alert.
- **M2** Folder picker (used by Transfer + Move) cannot create new
  folders inline. Add "+ New Folder" affordance to the picker UI in
  both `transfer.tsx` and `move-picker.tsx`.
- **M3** *(out of scope — needs firmware PNG conversion endpoint;
  requires a separate task to design the right format.)*
- **M4** Settings: no "Reset to defaults" affordance. Two options:
  (a) firmware-side `POST /api/settings/reset` + app entry, or
  (b) app-only "reset to known defaults from current array" (but app
  doesn't know the defaults). Pick (a).
- **M5** Generic error handling: 401 should redirect to QR pairing,
  not a generic "Failed: 401" toast. Add a small `apiErrorToast`
  helper that classifies common errors and routes to /scanner on 401.
- **M6** Discover: no hint when no devices found on /24 (could be
  /16). Add a one-line tip in the empty state.

### Low

- **L1** No tests — out of scope this round (separate testing task).
- **L2** A11y labels — add `accessibilityLabel` / `accessibilityRole`
  to all primary touchables.
- **L3** Hard-coded color literals — extract to a `ui-theme.ts` constant
  module (NOT a full theme rewrite; just a single source of truth for
  the 6 colors used).
- **L4** App strings hard-coded — out of scope this round (i18n needs
  its own task to pick a library + extract).
- **L5** WebSocket fast upload — out of scope (separate task).
- **L6** `useEffect` deps eslint-disabled — review and either add deps
  or convert to `useFocusEffect`.
- **L7** No "last refreshed at" indicator — add a small timestamp to
  the Device tab status panel.
- **L8** No retry rate limit — add 5s minimum between manual refresh
  taps.
- **L9** `pio check -e mofei --fail-on-defect high` not run on the
  recent firmware addition. Run it now and fix any defect ≥ medium.

## Out of scope

- L1, L4, L5, M3 (each requires its own task).

## Acceptance Criteria

- [ ] `cd app && npx tsc --noEmit` returns exit 0.
- [ ] `pio run -e mofei` succeeds (only minimal firmware changes for
      M4 reset).
- [ ] `pio check -e mofei --fail-on-defect high` succeeds.
- [ ] Each of H1, H3, M1, M2, M4, M5, M6, L2, L3, L6, L7, L8 visibly
      addressed.
- [ ] Existing screens still navigate without regression.

## Definition of Done

- One or two commits, scope-limited.
- Quality gates above pass.
- Task archived.

## Implementation order

1. Firmware (M4 reset endpoint) — small, self-contained.
2. App theme constants (L3) — touched by all subsequent edits.
3. App: H1, H3, M1, M2, M5, M6, L2, L6, L7, L8 in two waves.
4. Verification.
