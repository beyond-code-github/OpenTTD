# RW2: the joiner's arrival S-bend is sized by item rows like every other S-bend (DESIGN-G1 §16.4 RW2; RECON-per-row-code.md §2)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes2 (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/nodes2 checkout -B wt2-rw2 <HEAD>` (given at launch). Never use git stash. Another agent works in /home/pete/OpenTTD/.claude/nodes: never touch it. Use absolute paths.
- RW1 merged (48edcd0adb): row items from tokens via FindRowItems/node_item.
- The joiner edge's program is [Corner(index = approach item), Under/Over.., SBend(index = lane item)]. Its arrival S-bend keeps Elem::m = 1 because refresh (~:1338) sizes S-bends by item rows only for `is_row_edge(e)`, and its Y relation is hard-coded `eq(1, el.last, el.first, VY[side])` (~:2342, horizontal case). So a row between the Approach and Lane items contradicts (R7b's stop). Row-edge S-bends and Fans are already sized by rows (refresh ~:1338-1340; Fan ~:1325-1331, ~:2303-2308).

## Read first (once each)
CODINGSTYLE.md (C++ part); RECON-per-row-code.md §2; src/junction_topology.cpp refresh (~:1320-1345), the Y relations for S-bends (~:2300-2350), the readers of el.offs (~:1546, :1638, :1822, :1931, :2693: confirm they take the size from offs/m).

## Tests (record each red)
1. **`[topology]` "topology: a row gap between a joiner's approach and its lane":** on the T (and LL_RR if it has a joiner arrival S-bend), a y_floor that moves the Lane item one row further from its Approach item (y_floors: point 1 + lane item index, at its seed row + 1 on the side away from the approach; derive the side from the rows) places, with the arrival S-bend's m == 2, render + signals + CheckJunctionRules clean. Red: at HEAD the placement fails with the contradiction (record the reason chain).
2. **Outer, identity on every seed:** `[topologysweep]`, `[topologystretch]`, `[placeoracle]`, `[realterrainsweep]`, `[topoprojsweep]` outputs equal to launch HEAD's (times stripped), circuit byte-identical, EXCEPT the EDITSOUND lines of the edit sweep, where `failed` may drop and `clean` rise by the same amount (row-gap edits that now place): report each moved cell; `dirty` must stay 0.

## Changes (src/junction_topology.cpp)
1. refresh: the S-bend sizing by item rows applies to a horizontal S-bend on any edge (drop the `is_row_edge` guard for horizontal hin), with offs from bend_tiles as for row edges.
2. Delete the fixed `eq(1, el.last, el.first, VY[side])` for horizontal S-bends (~:2342); the S-bend's Y is then given by its end pins (the Corner on the approach item, the S-bend's last on the lane item) as for row edges. Keep the X relation (`eq(0, ...)`).
3. If the curve-gap or floor families read the S-bend's size from a constant 1 anywhere for joiners, they read m instead; say where.
4. TESTS.md row.

## Stop conditions
- Any seed or non-EDITSOUND number moves; any EDITSOUND cell gets dirty > 0; a reader takes the joiner S-bend's size from something other than m/offs and changing it moves a seed; anything not covered.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `'[topoproj]' 5`; the identity diffs; `tools/junction/jt gate build-rel` (count and time).
- One commit: `Change: Junction joiner arrival S-bend sized by item rows like every S-bend`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus the red reason chain and the EDITSOUND movements. When you have reported, stop.
