# Changelog

## 0.0.3 — 2026-08-21

Trust, recovery, mapping, and interface release.

- Contradictory observations now fail closed: derived **DIG**, **CAVE**, and odds disappear until counts agree, while conflict sites remain visible.
- Solver output is separated from authored map state and now carries exact model counts, deduction provenance, and explicit incomplete-analysis reasons.
- Unresolved odds reserve `0%` and `100%` for proof, using `<1%` / `>99%` and exact legal-layout fractions in status text.
- New, Open, and Close share a **Save / Discard / Cancel** guard; New maps are untitled, and dirty state follows undo/redo plus the persisted viewport.
- Saves use checked, durable temporary files and atomic replacement. Strict v2 maps preserve terrain, continuous supports, orientation, and view; v1 maps remain supported.
- Painted strokes and support replacements are one transaction. Redo, branch handling, no-op preservation, and accurate saved-state tracking are included.
- Added persistent keyboard cursor/focus, gap-free drag painting, pointer-anchored zoom, Fit Map, origin navigation, coordinate ticks, UI scaling, and scrollable responsive controls.
- Added Unicode native Open/Save As dialogs, `.hhmap` file drops, actionable errors, filename/dirty title state, and modal help/confirmation behavior.
- Added water terrain, authored-versus-proved caves, modeled protected-cave warnings, continuous radial support placement, overlap/removal-risk status, and rotatable tunnel estimates.
- Board states use non-color glyphs at every zoom. Contrast, status clipping, minimum-window layout, first-run help, and support confidence wording were revised.
- Added a deterministic brute-force solver oracle, strict schema/atomic-failure tests, headless document tests, offline core builds, sanitizer/Linux CI, pinned dependencies, and versioned macOS/Windows/Linux release bundles.

## 0.0.2 — 2026-08-19

Board language pass. Same solver.

- Green walls say **DIG**. Red walls say **CAVE**. Percents stay, labeled as cave-in chance.
- Open floor is slate, not the same brown as leftover odds.
- Hover a dust number: cyan outlines the walls that number still counts.
- Hover a percent: cyan outlines the other walls that share that number, and gold outlines the number itself.
- Status line states the action (DIG / CAVE / cave-in chance / open floor) instead of a bare `p=`.
- Sidebar legend matches those words.
- Help text explains the cyan highlight.

## 0.0.1 — 2026-08-19

First public release.

- Manual-entry C99 / raylib companion for Haven & Hearth cave-ins.
- Local minesweeper deduction with leftover odds.
- Open-floor mark (`G`) so Natural Gallery rooms are not entered as dust `0`.
- Wood / stone / beam / monumental support radii.
- macOS and Windows binaries.
