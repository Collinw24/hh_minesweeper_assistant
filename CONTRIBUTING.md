# Contributing

Contributions should keep HH Minesweeper a small, manual companion window for Haven & Hearth.

## Build and test

You need Git, CMake 3.16 or newer, a C99 compiler, `make`, and an internet connection when CMake first downloads raylib.

```sh
make
./build/hhms
make test
```

Run the window and exercise any controls or display behavior that your change affects. Before opening a pull request, confirm that both `make` and `make test` complete successfully.

## C style

- Write portable C99 and use the standard library unless an existing project dependency already provides the required facility.
- Follow the surrounding code: four-space indentation, braces on their own line for functions, and braces on the same line for control statements.
- Keep compiler warnings enabled and fix new warnings rather than suppressing them.
- Prefer small, direct functions and explicit data flow. Avoid avoidable allocation, copying, and per-frame work.
- Keep the solver and file format independent of raylib; GUI concerns belong in the application layer.

## Pull requests

- Keep each pull request focused on one behavior or closely related set of changes.
- Explain the user-visible problem and the behavior after the change.
- Add or update tests when solver behavior, persistence, or another observable contract changes.
- Update the README when controls, colors, build steps, releases, or mining guidance change.
- Do not include generated build directories, binaries, save files, or unrelated cleanup.
- Verify the GUI manually when the change affects the window, in addition to running `make test`.

## No client hooks

HH Minesweeper must remain manual-entry software. Do not add hooks into the Haven & Hearth client, process or memory inspection, network interception, client log scraping, input automation, or any other mechanism that reads from or controls the game client.
