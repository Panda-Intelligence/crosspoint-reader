# Repository Guidelines

## Project Structure & Module Organization
- `src/` contains firmware runtime code; `src/main.cpp` is the entry point.
- `src/activities/` holds screen-level flows (home, reader, network, settings, boot/sleep).
- `lib/` contains shared libraries (EPUB parsing, text/hyphenation, rendering helpers).
- `open-x4-sdk/` is a required submodule for hardware abstraction (display, input, storage, power).
- `scripts/` contains build-time generators (for example HTML and i18n generation).
- `test/` stores test assets and focused evaluation code (currently hyphenation-heavy).
- `docs/` contains contributor architecture notes and user-facing docs.

## Build, Test, and Development Commands
- `git submodule update --init --recursive` — initialize required submodules after cloning.
- `pio run` — build default firmware (`env:default`).
- `pio run --target upload` — flash firmware to the connected Xteink X4.
- `pio device monitor` — open serial logs for runtime debugging.
- `./bin/clang-format-fix` — apply repository formatting (`clang-format` 21+).
- `pio check --fail-on-defect low --fail-on-defect medium --fail-on-defect high` — run static analysis.
- `./test/run_hyphenation_eval.sh` — run the standalone hyphenation evaluation binary.

## Coding Style & Naming Conventions
- Language target is modern C++ (`-std=gnu++2a` / C++20 style).
- Follow `.clang-format` exactly (2-space indentation, 120-column limit); run formatter before commits.
- Keep class and activity files in paired `Name.h` / `Name.cpp` form (for example `ReaderActivity.h`).
- Use descriptive PascalCase for types, clear method names, and keep features scoped to existing modules.
- Do not hand-edit generated headers such as `src/network/html/*.generated.h`.

## Testing Guidelines
- Minimum local gate before a PR: format, `pio check`, and `pio run` (in that order).
- Validate behavior on-device for UI/input/network flows; include serial logs for regressions.
- Put new targeted C++ evaluations under `test/` and prefer `*Test.cpp` naming for executables/tests.

## Commit & Pull Request Guidelines
- Match existing Conventional Commit style: `feat:`, `fix:`, `refactor:`, `docs:`, `chore:`.
- Keep each branch and PR focused on one feature/fix area; branch from `master`.
- PR titles must be semantic (CI enforces this), e.g. `fix: avoid crash when opening malformed epub`.
- Fill out `.github/PULL_REQUEST_TEMPLATE.md`: goal, included changes, context/tradeoffs, and AI usage disclosure.
- For bug fixes, include reproduction steps and exact verification commands used.
<!-- TRELLIS:START -->
# Trellis Instructions

These instructions are for AI assistants working in this project.

Use the `/trellis:start` command when starting a new session to:
- Initialize your developer identity
- Understand current project context
- Read relevant guidelines

Use `@/.trellis/` to learn:
- Development workflow (`workflow.md`)
- Project structure guidelines (`spec/`)
- Developer workspace (`workspace/`)

If you're using Codex, project-scoped helpers may also live in:
- `.agents/skills/` for reusable Trellis skills
- `.codex/agents/` for optional custom subagents

Keep this managed block so 'trellis update' can refresh the instructions.

<!-- TRELLIS:END -->
