# E3: B's lanes are edges between nodes, routed like every other edge (D178)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Why
D178: every part of the hub is projected by the same `route_line`; per-part checks are deleted. After E1 and E2 only B's two lanes (the D80 doubled section, built by `BuildParallelogramLanes` from the skeleton's positions as fixed move strings) are laid without the router, and the fixed-part check's W-row test is their only terrain check. In the glossary's words (DESIGN-O4) each lane is a chain of edges between nodes: surface edges between the lane's straight-to-curve and curve-to-straight points, and a span edge (the bridge) between its two head nodes. This step routes each of them with `route_line`, nodes taken from the flat template, so the flat T stays identical while every stretch meets the terrain like any other edge.

## State
- Worktree `/home/pete/OpenTTD/.claude/e3`, branch `wt-e3`, head given at launch (feature/junction-tool with E1 and E2 merged; there is no growth stop yet). Create `build` (Debug) and `build-rel` (Release) like the main ones; build `openttd_test` only. If a build fails because a precompiled header is older than a system header, delete that .pch and rebuild.
- Tests only through `tools/junction/jt` (read `docs/junction-tool/TESTS.md`): `tools/junction/jt fast build-rel [projection] 5`, `tools/junction/jt fast build-rel [tmapnear] 5`, `tools/junction/jt fast build [junctionmap] 5`, final `tools/junction/jt gate build-rel`. OVER BUDGET is a stop. Never type tag filters by hand.

## Read first (once each)
`CODINGSTYLE.md`; the glossary, "Every part is an edge" and its routing-order notes in `docs/junction-tool/DESIGN-O4.md`; D80, D149, D174, D178 in `DECISIONS.md`; `BuildParallelogramLanes` and `ConstructJunctionTMap` (line-router path: `route_line`, the port edges, the fixed-part check, where the lanes are added to `fixed`, `branch_fence`, B->S's start `sl_x` and N->B's goals) in `src/junction_generator.cpp`; `JunctionPiece` and `JunctionRaster` (`on_bridge`, `trackdir`); in `src/junction_line_router.h` how a request starts on and ends at a bridge head (read the bridge move in `src/junction_line_router.cpp` only as far as needed).

## Definitions
- Lane rasters: `BuildParallelogramLanes`' two rasters, unchanged, are the flat template (the nodes' source). Lane A and lane B share their pieces from the E end up to the lane split (lane A's first pieces equal lane B's).
- Nodes of a lane: its first and last piece, every bridge head piece (near and far), and every piece where the lane changes between a straight-along-row piece and a curve or diagonal piece (the start and end of each S-bend). Lane A's nodes stop at the lane merge (its last piece, which lands on lane B's piece at x_lm); lane A's stretch before the lane split is lane B's and is not routed twice.
- Edges of a lane: each pair of consecutive nodes. A span edge if its two nodes are the two heads of one bridge; else a surface edge.

## Tests, red first (`[projection]`)
1. "a house on B's main line outside the junction is ignored": the test terrain with the W-row tile one column east of the flat T's last column `Blocked`; layout built, `clean`, equal tile for tile to the flat layout. Red: if it already passes after E1 (the W-row check is limited to the ports), say so and keep it as a guard.
2. "a house on B's lane inside the junction is reported by B": the test terrain with a tile of the flat T's lane B on the b row between its far head and the shift-back `Blocked` (pick it from the flat layout; assert it is not a bridge head and not a tile of any branch route). No layout or a clean layout that avoids it; every height's log names the lane edge that failed (`B lane edge failed at x,y`), (report the heights tried). Red: today the log says `fixed track on unusable tile`.
3. "a slope under B's straight stretch is ramped or reported": the W-row tiles of lane B's E stretch (between the E port and the lane split, two adjacent tiles not used by B->S's split) raised as an incline pair the line router can ramp (`build_on_slopes = true`). Either clean at some height or no layout with lane-edge failures logged; report which with the log.
4. Every existing test keeps its outcome; the flat identity (`[junctionmap]`) above all. The golden trace test gains the lane edges' lines: update `BUILT_HEIGHT_LINES` and the expected lines from the actual run with a one-line comment (E3, D178) and report the count; any other change in that trace is a stop.

## Changes (`src/junction_generator.cpp`, `ConstructJunctionTMap`, line-router path only)
1. After `BuildParallelogramLanes` returns, compute each lane's nodes and edges as defined (a static helper next to `BuildParallelogramLanes`, with a doc comment, returning for one raster the node indices).
2. Route the edges in this order: lane B's surface edges, then lane A's surface edges, then the span edges of both lanes (surface first, then span edges: DESIGN-O4 routing order). Each with `route_line` on `fixed`: start `{node piece tile, node piece trackdir}`, goal `{next node piece tile, its trackdir}`, `min`/`max` the bounding box of the two node tiles (rows w_row .. b_row), other fields from `base`, blocked as `branch_fence`'s rows 1 .. w_row - 1 part. If the router cannot start or end on a given piece (for example a bridge head), stop and report what it requires.
3. On failure: `say(fmt::format("B lane edge failed at {},{}", x, y))` with the edge's first node tile, and continue to the next height (as the other routed edges do).
4. The routed rasters replace `lanes->first` / `lanes->second` wherever they are used after this point (added to `fixed`, B->S's start pieces, N->B's goals, the lane part of `branch_fence`).
5. Delete the fixed-part check entirely on the line-router path (its last part, the W row, is now routed); delete its comment and the `fixed_parts` layout there. The old-router path is unchanged.

## Do not
No change to the skeleton solver, `BuildParallelogramLanes`, the router, the old-router path, A, the port edges, the box growth, `GenerateJunction`, signal pass, checker, the GUI, `junction_organic.*`, savegame, network or command code. No `std::to_string`/`std::printf` in tests; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
- The flat identity or any other existing test changes outcome (name it and its failure), apart from the allowed golden-trace lines.
- A lane edge does not route on flat ground.
- OVER BUDGET; anything not covered.

## Gates
0 warnings in both builds; the jt runs above pass. One commit on `wt-e3`: `Change: Junction T map routes B's lanes between their nodes`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence; functions added; nodes per lane on the test spec (count and kinds); tests 2 and 3 outcomes with logs; golden trace change; jt lines with times; BENCH (Release, test spec flat: micros, micros_flat, micros_route) before and after; stops; `git status --porcelain`, `git log -1`.

When you have reported, stop.
