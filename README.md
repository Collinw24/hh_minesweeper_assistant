# HH Minesweeper

HH Minesweeper is a C99 and raylib side window for recording cave-in information while mining in [Haven & Hearth](https://www.havenandhearth.com/). You enter what happened in the game; the board identifies tiles that are logically safe, tiles that must contain a cave-in, and unresolved local odds.

The program is a manual notebook. It does not connect to, inspect, or control the Haven & Hearth client.

## How mining dust works

After mining a tile, the dust you receive counts cave-ins in its eight neighboring tiles, including diagonals:

- `0.01 kg` means one adjacent cave-in, `0.02 kg` means two, and so on through `0.08 kg`.
- A count of `0` means that you mined the tile and no dust fell. All eight neighboring tiles are therefore safe.
- A tile that is itself a cave-in does not produce a dust count of its own.

Enter the digit from `0` through `8`, not the dust weight. As counts and flags are added, the board recalculates deductions immediately.

## First session

1. Start HH Minesweeper and keep it beside the game window. The board opens with tile `(0,0)` outlined as the origin; choose a consistent in-game tile to represent it.
2. Mine a tile in Haven & Hearth and collect its dust. Hover the corresponding board square and press `0` through `8`. The key enters that count immediately. You can also select a count in the side panel and left-click or drag across squares.
3. When you open an existing gallery floor rather than mining a wall, use the `Floor` tool as described in the safety notes below.
4. Read the updated board. Green squares are proved safe and red squares are cave-ins. A numbered square is an observation you entered, not a recommendation from the program.
5. If you observe a cave-in that is not already proved, hover it and press `F`, or right-click it, to add a flag. Press `X` to correct an entry and `U` to undo the last edit.
6. Edit the filename in the side panel if needed, then press `S` to save the map. Press `L` later to load it.

The board is unbounded. Pan and zoom as the mine grows; coordinates in the status bar help keep the notebook aligned with the game.

## Safety notes

- Never enter `0` on a Natural Gallery room that you did not mine. Mark an opened gallery tile with `Floor` or `G`; it carries no dust observation.
- The first tile mined into a wall can randomly cave in before any neighboring dust could warn you.
- Water can conceal a cave-in, so visible terrain alone may not account for a dust count.
- Available supports are wood, stone, mine beam, and monumental. Their radii are 9, 11, 13, and 30 tile units respectively, and a support protects a tile only when the radius covers that tile's center. Dust still appears beneath support coverage and must be recorded normally.

## Colors and marks

| Appearance | Meaning |
|---|---|
| Dark wall square | No observation has been entered |
| Brown floor square | Opened gallery floor |
| Dark square with a colored number | Mined tile and its entered dust count |
| Green | Proved safe by the entered information |
| Red with `X` | Proved or manually flagged cave-in |
| Brown-orange heat with a percentage | Local leftover cave-in probability among unresolved tiles; it is not a suggestion to click |
| Magenta | Conflict: the entered counts and flags cannot all be true |
| Blue wash | The tile center is covered by a support |
| Gold outline | Board origin or current hover position |

## Controls

Most tools can be selected in the side panel and painted with the left mouse button.

| Input | Action |
|---|---|
| Hover + `0`–`8` | Enter the mined tile's dust count |
| `G` / `Floor` | Mark opened gallery floor |
| `F` or right-click | Add or remove a cave-in flag |
| `X` or Backspace | Erase the tile |
| Left-click or left-drag | Apply the selected tool |
| Shift-click | Place or remove the selected support |
| `[` / `]` | Select the previous or next support type |
| Middle-drag or Space + left-drag | Pan the board |
| `W` `A` `D` or arrow keys | Pan the board (`S` saves) |
| Mouse wheel | Zoom |
| `U` | Undo |
| `S` | Save the current `*.hhmap` file |
| `L` | Load the named `*.hhmap` file |
| `N` | Start a new map; repeat to confirm if there are unsaved changes |
| `T` | Toggle whether the window stays on top |
| `R` | Reset the camera |
| `H` or `/` | Toggle the in-program help |

## Download a release

Tagged releases publish `hhms-macos` and `hhms-windows.exe` binaries through GitHub Actions. Download the file for your platform from the [GitHub Releases page](https://github.com/Collinw24/hh_minesweeper_assistant/releases). On macOS, make the downloaded file executable with `chmod +x hhms-macos`, then run `./hhms-macos`; on Windows, run `hhms-windows.exe`.

## Build from source

You need Git, CMake 3.16 or newer, a C99 compiler, `make`, and an internet connection for the first configuration. CMake downloads raylib 5.5 during that first build.

```sh
make && ./build/hhms
```

Run the non-windowed test suite with:

```sh
make test
```

## Project

Contributions are described in [CONTRIBUTING.md](CONTRIBUTING.md). HH Minesweeper is maintained by [Collinw24](https://github.com/Collinw24) and released under the [MIT License](LICENSE).
