# OC1: occupancy, unrelated tracks never share or touch a tile (DESIGN-N3 §7; D193)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
Worktree `/home/pete/OpenTTD/.claude/oc1`, branch `wt-oc1`, head given at launch. Create `build` (Debug) and `build-rel` (Release) (clang-18, ccache); build `openttd_test` only. Tests only through `tools/junction/jt`: `'[nodes]'` (budget 1 s Release, 5 s Debug); `jt gate build-rel` once before the commit. Output in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/oc1/`.

## Read first (once each, offset/limit)
`CODINGSTYLE.md`; `docs/junction-tool/DESIGN-N3.md` §5-§7; `src/junction_nodes.h`; `src/junction_nodes.cpp` (AddJunctionNodeLateConstraints, NodeDiagonalRaster, JunctionNodeSnap, RenderJunctionNodeTracks, SolveJunctionNodes); `src/tests/junction_nodes.cpp` lines 590-680 (DetourFixture, HasTrackAt) and the N0c2 tests at the end.

## Changes
Header:
```cpp
/**
 * Add the occupancy chain of DESIGN-N3 §7 for the current rows (phase 3, after AddJunctionNodeLateConstraints).
 * @param values Every Y variable's fixed value (X entries ignored).
 * @param[out] row On failure, the layout row (current) where it failed.
 * @return False when two elements overlap in the flat order on one row, or a constraint is infeasible.
 */
bool AddJunctionNodeOccupancy(const JunctionNodeTemplate &tpl, JunctionPositions &positions, const std::vector<int> &values, int &row);
```
JunctionNodeFailure::Kind gains `Occupancy` (column = the row; update the `column` member's comment).
`.cpp`, exactly DESIGN-N3 §7:
- An element = (current row, flat row, lo, hi), with lo and hi each a (variable, offset) pair so that x = values[var] + offset at any values (offsets as the template's x = value(cls) + tile.x - flat[cls]; for raster tiles add the raster offset at the current diagonal length; the X-run interior lo/hi are x_p + d and x_q - d ordered by x). Flat lo/hi = the same at tpl.flat values (for a free diagonal, the raster at its flat length: an element present only at the current length has no flat tile; use the flat x of p plus the current offset for its flat lo/hi, and treat it as on a different flat row: gap 2).
- Elements: every non-on_bridge JunctionNodePiece; every bridge head (start and end, the bridge's cls/ycls); every free X run interior (skip when empty); every interior tile of a free Y run; every interior raster tile of a free diagonal (raster as the render draws it).
- Groups: elements with identical flat tile and identical current row: equal x (lo of each to the first, two diffs k 0) and one chain entry.
- Per current row: sort by flat lo x (ties by flat hi); consecutive a, b: flat hi_a >= flat lo_b -> return false with row; else add lo_b - hi_a >= gap (gap = min(2, flat lo_b - flat hi_a) when a and b have the same flat row, else 2); AddDifference false -> return false with row.
- JunctionNodeSnap calls it after AddJunctionNodeLateConstraints (false -> return {}). SolveJunctionNodes calls it after (2c) and before (3): false -> failure Occupancy with column = row.

## Tests, red first (`[junction][nodes]`). Red: AddJunctionNodeOccupancy returns true and adds nothing; record each failure.
1. "two lines forced onto one row collide" (inner): DetourFixture(2, 4); positions over tpl.flat.size(); AddJunctionNodeConstraints; add y_b - y_a = 0 for the two lines' Y variables (two diffs) (the first point of each line); REQUIRE true; CHECK JunctionNodeSnap returns empty (red: it returns values and the render has fewer than 30 track tiles).
2. "two lines forced onto adjacent rows stay apart" (inner): as 1 with y_b - y_a = 1; CHECK the snap returns values and the render has 30 track tiles (two full lines, no shared tile).
3. Every existing [nodes] test passes unchanged, including N0c2's 100-196 km/h test (occupancy must not reject the legal flat geometry), the house/hub/flat searches and N0c1's deeper-box test.

## Do not
No change to junction_positions, junction_chain, the generator, the router, the checker, the signal pass. No vertical edge-contact rule. No prio rows. No `std::to_string`/`std::printf`; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
- An existing test fails with Occupancy (or the snap returns empty): report the row, the two elements (flat tiles, kinds, classes) verbatim; do not change the rule.
- OVER BUDGET; anything not covered.

## Gates
0 warnings; `jt fast build '[nodes]' 5`, `jt fast build-rel '[nodes]' 1`, `jt gate build-rel`. One commit on `wt-oc1`: `Feature: Junction node occupancy chain (DESIGN-N3 §7)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence; members added; number of occupancy diffs on the flat T (WARN it in test 3's place: add one WARN in test 1 of N0c1 "a deeper box keeps the flat T clean" is NOT allowed; instead WARN it in the new test 2 for the fixture and report the flat-T count from a temporary local run, not committed); `[nodes]` times; gate; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
