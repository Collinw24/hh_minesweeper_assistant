# Changelog

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
