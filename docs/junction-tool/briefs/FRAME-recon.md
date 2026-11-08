# Brief FRAME-recon: inventory of the generator's fixed frame constants (read-only)

You execute this brief and nothing else. Don't edit any source file. Write exactly one file, `docs/junction-tool/RECON-frame.md`, and don't commit it (the orchestrator commits it). Keep your context small: grep and read with offset and limit.

## 1. Why

The junction generator works in a fixed canonical frame:
- 64 tiles wide (x 0..63);
- hub at working (30, 0), so `xI = 30`;
- the terrain snapshot is 64×43 from origin (0, −2), plus a margin;
- the router's search box is `base.min = (1, 0)` to `base.max = (xO + 16, yb)`;
- lane programs start at `tile(63, w_row)` and end at x = 0;
- various windows use 61, 62 and 63.

Long prios (D102: about 21 tiles; D108: no split inside a prio) need a wider frame, and the fixed constants block generation at `prio_tl` ≥ 2 (see the D109 stop in `docs/junction-tool/STATUS.md` and D109 in DECISIONS). We need every place that hard-codes or assumes the frame.

## 2. Search

- `src/junction_generator.cpp`, `src/junction_generator.h`
- `src/junction_game.cpp`/`.h` (`MakeJunctionTerrain`, `GenerateJunctionNear`, `FitJunctionLibraryNear`, offsets)
- `src/junction_terrain.h`
- `src/junction_router.cpp` (grid bounds)
- `src/junction_gui.cpp` (hub and anchor maths)
- `src/console_cmds.cpp` (`junction_export`)
- `src/tests/*junction*` (fixtures that build terrains of 64×43 or use hub 30)

Grep for: `30`, `63`, `62`, `61`, `64`, `43`, `xI`, `xO`, `-2`, `origin`, `width`, `height`, `tile(63`, `+ 16`, `- 16`, and the comment "working coordinate".

## 3. Output: `RECON-frame.md`

A table: file:line, the constant or expression, what it means (frame width, hub x, terrain height, window edge, search bound, and so on), and what it depends on.

Then:
1. The minimal set of frame parameters that would replace them all. For example: frame width W, hub x `XH`, terrain height, and the search margins. For each, the formula it would need in terms of the spec: tl, prio coverage, sd, gaps.
2. Every place where an assumption is implicit (for example, "lane B search loops p ≤ 63").
3. The risk list: which tests or fixtures hard-code the frame and would need updating.
4. A proposed order of changes, in 2–4 small commits, each keeping the golden output identical at prio 1.

## 4. Report

- the file path
- a 10-line summary
- the proposed parameter set and formulas

When you have reported, stop.
