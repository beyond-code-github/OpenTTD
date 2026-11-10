# G4b: the main line and B's W row are checked only where the junction is built (D170a, D177)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Why
`ConstructJunctionTMap`'s fixed-part check (G3b) tests row 0 and the W row across all of the frame's 64 columns before routing, but `FinishJunctionCandidate` crops the layout to far fewer columns. In game, a hill or a house 20 tiles west of the junction therefore gives no layout, though the built junction would never touch it. Row 0 and the W row are to be checked on the cropped layout instead. The stubs keep their check before routing (an obstacle there grows the box).

## State
- Worktree `/home/pete/OpenTTD/.claude/g4b`, branch `wt-g4b`, head given at launch (feature/junction-tool with G4 merged: `JunctionTMapResult::hub`, the plane from the hub tile, D177). Create `build` (Debug) and `build-rel` (Release) like the main ones; build `openttd_test` only.
- Tests only through `tools/junction/jt` (read `docs/junction-tool/TESTS.md`): `tools/junction/jt fast build-rel [projection] 5`, `tools/junction/jt fast build-rel [tmapnear] 5`, `tools/junction/jt fast build [junctionmap] 5`, final `tools/junction/jt gate build-rel`. OVER BUDGET is a stop. Never type tag filters by hand.

## Read first (once each)
`CODINGSTYLE.md`; `ConstructJunctionTMap` (whole: the fixed-part check, the `FinishJunctionCandidate` call and `result.hub`) and `FinishJunctionCandidate` (how it crops and what `hub_out` means) in `src/junction_generator.cpp`; the `[projection]` tests in `src/tests/junction_map.cpp`, in particular "an obstacle on the main line is reported".

## Tests (`[projection]`), red first
1. "an obstacle on the main line outside the junction is ignored": the test terrain with the tile (3, 0) `Blocked`. First assert, from `SharedFlatT()` (its `hub` and `layout->size`), that frame x 3 lies west of the flat layout's columns (frame x of layout column 0 is `frame.hub_x - hub.x`). `ConstructJunctionTMap(spec, &terrain, SharedFlatT().height, true)`: layout built, `clean`, equal tile for tile to the flat layout. Red: today no layout, log `fixed track on unusable tile 3,0`.
2. Change the existing "an obstacle on the main line is reported": the blocked tile becomes (xI - 1, 0) (xI = `MakeJunctionFrame(spec).hub_x`; first assert it lies inside the flat layout's columns); expect no layout and the log line `fixed track on unusable tile {xI - 1},0`. One-line comment citing this brief.
3. "an obstacle on B's W row outside the junction is ignored": as test 1 on the W row (the row the generator calls `w_row`; take it from the flat layout or the generator's rule, say which) at the column one east of the flat layout's last column. Red: today no layout.

## Change (`src/junction_generator.cpp`, `ConstructJunctionTMap`)
1. The pre-routing check keeps only the stubs: a fixed-part tile on row 0 or the W row is no longer tested there. Keep the stub branch as it is (log, `stub_blocked`, next height).
2. After `FinishJunctionCandidate` returns a layout and only when `terrain` is set: for every track tile of the finished layout whose frame row is 0 or `w_row` (frame coordinates = layout coordinates + (`frame.hub_x - result.hub.x`, `-result.hub.y`)), `terrain->UsableAt(fx, fy, plane)` must hold; else log `fixed track on unusable tile fx,fy` (frame coordinates, the same text as before) and return the result with no layout and the timings set, as the pre-routing check did (growing the box cannot move row 0 or the W row). Comment: why after the crop (the frame is wider than the built junction), and that routed pieces on these rows were already checked by the line router, so the test only bites on the fixed main-line and lane track.
3. Nothing else.

## Do not
No change to the router, the old-router path, the stub check, `FinishJunctionCandidate`, `GenerateJunction`, `junction_game.*`, the GUI, decisions, skeleton, signal pass, checker, `junction_organic.*`, savegame, network or command code. No `std::to_string`/`std::printf` in tests; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
- Test 1 or 3's layout is not equal to the flat one, or not clean.
- An existing test other than the one in test 2 changes outcome (name it and its failure).
- OVER BUDGET; anything not covered.

## Gates
0 warnings in both builds; the jt runs above pass. One commit on `wt-g4b`: `Change: Junction T map checks the main line only where it is built`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence (each test, the failure seen); jt lines with times; stops; `git status --porcelain`, `git log -1`.

When you have reported, stop.
