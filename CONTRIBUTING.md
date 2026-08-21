# Contributing

Contributions should keep HH Minesweeper a small, manual companion window for Haven & Hearth.

## Build and test

GUI builds need Git, CMake 3.16+, C and C++ compilers, and the platform GUI development libraries required by raylib and Native File Dialog Extended. The first GUI configuration downloads content-locked dependencies.

```sh
make
./build/hhms
```

Core and headless application tests use a separate GUI-disabled build and require no GUI download:

```sh
make test
```

Sanitizer coverage is available on supported Clang/GCC platforms:

```sh
make test-sanitize
```

Run the window and exercise every control, display state, file transition, or support behavior affected by a GUI change. Before opening a pull request, confirm the focused tests and GUI build complete successfully.

## Architecture

- `src/hhms.c` owns authored tiles, terrain, supports, geometry queries, and session edit history.
- `src/solver.c` takes a const authored map and rebuilds `HhmsAnalysis`; solver output is never persisted.
- `src/persistence.c` strictly loads v1/v2 maps and atomically writes canonical v2 files.
- `src/app.c` owns headless document state, dirty baselines, pending destructive actions, and status formatting.
- `src/gui.c` processes raylib input before drawing; draw functions must remain read-only.
- `src/dialogs.c` is the only native file-dialog boundary.

Do not reintroduce authored/derived state mixing, mutate documents from rendering, hide solver limits, or serialize session history.

## C style

- Write portable C99 in application code. GUI dependencies may compile their own C++, Objective-C, or platform sources.
- Follow the surrounding code: four-space indentation, braces on their own line for functions, and braces on the same line for control statements.
- Keep compiler warnings enabled and fix new project warnings rather than suppressing them.
- Prefer small, direct functions and explicit data flow. Avoid allocation, copying, or per-frame work when stable storage or cached queries suffice.
- Keep solver, persistence, and headless application behavior independent of raylib.
- Treat continuous support geometry and directional estimates as distinct confidence levels; estimated tunnel footprints must never produce a safety claim.

## Tests

- Add or update behavior tests for solver, persistence, history, document transitions, or formatting changes.
- The deterministic brute-force oracle must remain independent from production solver logic.
- Persistence failure tests must prove that the previous map, path, history, saved state, and destination file remain unchanged.
- UI changes require a live smoke check at the minimum window size and a wider layout.
- Tests must be deterministic, isolated, and clean temporary files on success and failure.

## Pull requests

- Keep each pull request focused on one behavior or closely related set of changes.
- Explain the user-visible problem and the behavior after the change.
- Update [README.md](README.md) when controls, colors, file format, build steps, releases, limits, or mining guidance change.
- Update [CHANGELOG.md](CHANGELOG.md) with every user-visible change.
- Do not include generated build directories, binaries, save files, or unrelated cleanup.

## No client hooks

HH Minesweeper must remain manual-entry software. Do not add hooks into the Haven & Hearth client, process or memory inspection, network interception, client log scraping, input automation, or any other mechanism that reads from or controls the game client.
