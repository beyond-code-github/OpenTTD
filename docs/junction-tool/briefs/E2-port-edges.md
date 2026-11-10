# E2: the N and S stubs are port edges, routed like every other edge (D178)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Why
D178: every part of the hub is projected by the same `route_line`; the per-part checks are deleted. After E1 the main line A is routed; the N and S stubs (`STUB` straight pieces from the S and N ports at the box's last row) are still laid as fixed track by `base_layout` and checked by the "stub on unusable tile" branch of the fixed-part check. In the glossary's words (DESIGN-O4) each stub is a surface edge from a port node to the first inner node: the S port edge carries A->S and B->S (B->S merges onto it), the N port edge carries N->A and N->B (N->B splits from it). This step routes them.

## State
- Worktree `/home/pete/OpenTTD/.claude/e2`, branch `wt-e2`, head given at launch (feature/junction-tool with E1 and GS1 merged: A routed between its ports, `growth_can_help`). Create `build` (Debug) and `build-rel` (Release) like the main ones; build `openttd_test` only. If a build fails because a precompiled header is older than a system header, delete that .pch and rebuild.
- Tests only through `tools/junction/jt` (read `docs/junction-tool/TESTS.md`): `tools/junction/jt fast build-rel [projection] 5`, `tools/junction/jt fast build-rel [tmapnear] 5`, `tools/junction/jt fast build [junctionmap] 5`, final `tools/junction/jt gate build-rel`. OVER BUDGET is a stop. Never type tag filters by hand.

## Read first (once each)
`CODINGSTYLE.md`; the glossary, "Every part is an edge" and D178 (`docs/junction-tool/DESIGN-O4.md`, `DECISIONS.md`); `ConstructJunctionTMap` in `src/junction_generator.cpp`, line-router path: `base_layout`, `stub_pieces`, the fixed-part check's stub branch, A's routing (E1), `route_line`, `growth_can_help` and its failure sites, the A->S goal, N->A start, B->S goals and N->B starts; the `[projection]` tests in `src/tests/junction_map.cpp` that mention the stub (search "stub").

## Tests, red first (`[projection]`)
1. "a house on the S port edge grows the box": the test terrain with the tile (xO, flat_height - 3) `Blocked` (on the flat T's S stub; xO = hub_x + branch_gap + 1). Layout built at a height above the flat one, `clean`, the blocked tile unused; the log line for the flat height ends `S port edge failed`. Red: today the log says `stub on unusable tile`.
2. "a slope under the S port edge is ramped": the test terrain with the S port edge's two upper tiles at the flat height (rows ys and ys + 1, column xO) raised to an incline the line router can ramp (z 0..1 incline as `SetRampRow` describes, rising along -y, with `build_on_slopes = true`), every other tile flat. Either a clean layout at the flat height whose S port edge ramps, or a clean layout at a greater height; report which with the log. Red: today `stub on unusable tile`.
3. Every existing test keeps its outcome, the flat identity (`[junctionmap]`: line-router T equals the old router's T) and the golden trace test included. The golden trace test may gain the two port edges' lines at the built height: update `BUILT_HEIGHT_LINES` and the expected lines from the actual run, with a one-line comment (E2 routes the port edges, D178), and report the change; any other change in that trace is a stop.

## Changes (`src/junction_generator.cpp`, `ConstructJunctionTMap`, line-router path only)
1. At each height, before A, route the S port edge and the N port edge with `route_line` on the layout being built:
   - S: start `{tile(xO, ys), Trackdir::Y_SE}`, goal `{tile(xO, yb), Trackdir::Y_SE}`, `min = tile(xO, ys)`, `max = tile(xO, yb)`.
   - N: start `{tile(xI, yb), Trackdir::Y_NW}`, goal `{tile(xI, ys), Trackdir::Y_NW}`, `min = tile(xI, ys)`, `max = tile(xI, yb)`.
   - The other request fields from `base`; `route_line` adds `last_row` as today (the port tiles are not in it). On failure `say("S port edge failed")` / `say("N port edge failed")`, apply `growth_can_help` with no dependent tiles (the stub rows are at or below ys - 2, so growth continues while the box can grow), and continue to the next height.
   - Their rasters are added to the layout where `base_layout` added the stubs.
   If the router cannot route a start and goal that are the ends of the same straight piece sequence (for example, it needs goal and start to be different tiles at least two pieces apart), stop and report.
2. `base_layout` lays nothing on the line-router path (E1 already removed row 0 there); remove the line-router path's use of it, and keep it for the old-router path.
3. `stub_pieces` on the line-router path: B->S's goals and N->B's starts are taken from the port edges' rasters (the pieces on rows ys .. yb - 1 for S as B->S goals with `Trackdir::Y_SE`; rows ys + 1 .. yb - 1 for N as N->B starts with `Trackdir::Y_NW`, as today's `first_y` arguments), only pieces whose track is the straight Y track. The old-router path keeps `stub_pieces`.
4. The fixed-part check: delete the stub branch and `stub_blocked`; on the line-router path what remains is the W row between the ports (until E3). Update the comment above it (D178: the stubs are routed port edges).
5. Nothing else.

## Do not
No change to the router, the old-router path, A's routing, B's lanes, `growth_can_help`'s rule, `GenerateJunction`, the skeleton, signal pass, checker, the GUI, `junction_organic.*`, savegame, network or command code. No `std::to_string`/`std::printf` in tests; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
- The flat identity or any other existing test changes outcome (name it and its failure), apart from the allowed golden-trace lines.
- Test 1 not clean or not built: report the log per height and violations (rule, tile, message).
- OVER BUDGET; anything not covered.

## Gates
0 warnings in both builds; the jt runs above pass. One commit on `wt-e2`: `Change: Junction T map routes its port edges`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence (each test, the failure seen); tests 1 and 2: height, heights_tried, log; golden trace change; jt lines with times; BENCH (Release, test spec flat: micros, micros_flat, micros_route); stops; `git status --porcelain`, `git log -1`.

When you have reported, stop.
