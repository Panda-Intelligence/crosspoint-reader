# Quality Guidelines

> Code quality standards for frontend development.

---

## Overview

<!--
Document your project's quality standards here.

Questions to answer:
- What patterns are forbidden?
- What linting rules do you enforce?
- What are your testing requirements?
- What code review standards apply?
-->

(To be filled by the team)

---

## Forbidden Patterns

<!-- Patterns that should never be used and why -->

(To be filled by the team)

---

## Required Patterns

<!-- Patterns that must always be used -->

### Preserve Dashboard Shortcut Persistence Compatibility

When changing Mofei Dashboard shortcuts, keep the persisted shortcut key/id contract separate from the visible slot
contract.

#### 1. Scope / Trigger

Use this rule whenever changing `DashboardShortcutId`, default dashboard shortcuts, dashboard rendering/opening logic, or
`DashboardShortcutStore` repair behavior.

#### 2. Signatures

```cpp
enum class DashboardShortcutId : uint8_t;
static constexpr size_t DashboardShortcutStore::SLOT_COUNT;
bool DashboardShortcutStore::isAvailable(DashboardShortcutId id);
const DashboardShortcutDefinition* DashboardShortcutStore::definitionFor(DashboardShortcutId id);
bool DashboardShortcutStore::idFromKey(const char* key, DashboardShortcutId* outId);
```

#### 3. Contracts

- `SLOT_COUNT` is the visible Dashboard slot count, not automatically the enum count.
- Persisted string keys in `shortcuts.json` may outlive visible shortcuts.
- Retired shortcuts should remain parseable as legacy keys/ids when needed, but `isAvailable()` must exclude them from
  defaults, rendering, cycling, and repaired lists.
- Dashboard open/render code must consume only available shortcuts from the normalized store.

#### 4. Validation & Error Matrix

| Condition | Required behavior |
| --- | --- |
| Stored config contains retired shortcut key/id | Repair to defaults or first available unused shortcut |
| Stored config has wrong slot count | Restore and save defaults |
| Shortcut id is valid enum but retired | `definitionFor()` returns `nullptr`; store repair excludes it |
| Visible shortcut is File Browser or Settings | Keep protected replacement behavior |

#### 5. Good/Base/Bad Cases

- Good: Keep `DesktopHub` as a legacy enum/key while excluding it via `isAvailable()`.
- Base: Add a new visible shortcut by updating definition, key, default list, and repair/cycle availability together.
- Bad: Renumber enum values or tie `SLOT_COUNT` directly to `DashboardShortcutId::Count` after retiring visible items.

#### 6. Tests Required

- Run `git diff --check`.
- Run `pio run -e mofei`.
- Run `pio check -e mofei --fail-on-defect low --fail-on-defect medium --fail-on-defect high` when shortcut repair,
  Dashboard touch handling, or rendering changes.

#### 7. Wrong vs Correct

Wrong:

```cpp
static constexpr size_t SLOT_COUNT = static_cast<size_t>(DashboardShortcutId::Count);
```

Correct:

```cpp
static constexpr size_t SLOT_COUNT = 9;
bool DashboardShortcutStore::isAvailable(const DashboardShortcutId id) {
  return isValid(id) && id != DashboardShortcutId::DesktopHub;
}
```

---

## Testing Requirements

<!-- What level of testing is expected -->

(To be filled by the team)

---

## Code Review Checklist

<!-- What reviewers should check -->

(To be filled by the team)
