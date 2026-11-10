# G4: the projected T map in the in-game junction tool (D173, D177)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Why
The engineer wants to sculpt ridges in game and try the tool on them. The tool's refine tier (`GenerateJunctionNear`, run on a worker thread by `BuildJunctionWindow::StartJob` in `src/junction_gui.cpp`) still calls the old `GenerateJunction`. The line-router T map (`ConstructJunctionTMap(spec, &terrain, std::nullopt, true)`) is identical to it on flat ground and is projected onto terrain, but it builds at plane 0 only, and its result has no hub offset, which the GUI needs to place the ghost.

## State
- Worktree `/home/pete/OpenTTD/.claude/g4`, branch `wt-g4`, head given at launch (feature/junction-tool). Create `build` (Debug) and `build-rel` (Release) like the main ones; build `openttd_test` only.
- D177 in `docs/junction-tool/DECISIONS.md` (main checkout): "the T map's plane is the ground height (z_max) of the hub tile, working (hub_x, 0), when that tile is Buildable and not steep; otherwise no layout."
- Tests only through `tools/junction/jt` (read `docs/junction-tool/TESTS.md`): `tools/junction/jt fast build-rel [projection] 5`, `tools/junction/jt fast build-rel [tmapnear] 5` (new tag, below), `tools/junction/jt fast build [junctionmap] 5`, final `tools/junction/jt gate build-rel`. OVER BUDGET is a stop. Never type tag filters by hand.

## Read first (once each)
`CODINGSTYLE.md`; `ConstructJunctionTMap` (whole: the `base.plane = 0` line, the fixed-part check's `UsableAt(..., 0)`, the `FinishJunctionCandidate` call) and `FinishJunctionCandidate`'s `hub_out` in `src/junction_generator.cpp`; `JunctionTMapResult` and `JunctionGenerateResult` in `src/junction_generator.h`; `GenerateJunctionNear` in `src/junction_game.cpp`; the `GenerateJunctionNear` tests and `MakeNearTestTerrain` in `src/tests/junction_game.cpp`; the `[projection]` tests and `MakeRidgeTerrain`/`SetRampRow`/`SharedFlatT` in `src/tests/junction_map.cpp`.

## Tests, red first
In `src/tests/junction_map.cpp`, tag `[projection]`:
1. "the T map builds at the hub's ground height": the test spec's flat test terrain (the file's helper) with every tile set to z_min = z_max = 2. `ConstructJunctionTMap(spec, &terrain, std::nullopt, true)`: layout built, `clean`, and equal tile for tile (tracks, bridges, signals) to `SharedFlatT()`'s layout. Red: today the log says `fixed track on unusable tile` (plane 0).
2. "an unbuildable hub tile gives no layout": the flat test terrain with the tile at working (hub_x, 0) `Blocked` (hub_x from `MakeJunctionFrame(spec)`): no layout, log contains `hub tile not buildable`. Red: today the log says `fixed track on unusable tile`.
3. "the T map reports the hub offset": with no terrain, the T map's `hub` equals `GenerateJunction(spec, nullptr).hub` (test spec). Red: the member is zero.

In `src/tests/junction_game.cpp`, new tag `[junction][tmapnear]`:
4. "GenerateJunctionNear builds the projected T map over a ridge": `MakeNearTestTerrain()` with the ridge of the `[projection]` ridge test (same rows, heights and inclines relative to the flat height and b row; copy the small helper as a static function in this file if it is static in the other one); `build_on_slopes = true`. `GenerateJunctionNear(spec, terrain, 0, false, cancel)`: offset (0, 0), layout equal tile for tile to `ConstructJunctionTMap(spec, &terrain, std::nullopt, true).layout`, `hub` equal to that result's `hub`, `error == JunctionGenerateError::None`. Red: today it returns the old generator's result (record what: no layout, or a different layout).
Report each tag's time; add `[tmapnear]` to the TESTS.md table with its measured Debug and Release times (budget: Release 5 s).

## Changes
1. `src/junction_generator.h`: `JunctionTMapResult` gains `TileIndexDiffC hub{}; ///< The offset, in the layout, of the frame's hub point (hub_x, 0), as JunctionGenerateResult::hub.`
2. `src/junction_generator.cpp`, `ConstructJunctionTMap`:
   a. Pass `&result.hub` as `hub_out` to `FinishJunctionCandidate` (both paths, if both call it).
   b. Plane (D177): before the height loop, `int plane = 0;` and, when `terrain` is set: if the tile at (xI, 0) is null, not `Buildable` or `steep`, log `"hub tile not buildable"` (one line, not per height) and return the result with no layout; else `plane = tile->z_max`. Replace `base.plane = 0` with `base.plane = plane` and the fixed-part check's `UsableAt(jt.offset.x, jt.offset.y, 0)` with `plane`. Update the two comments that say "plane 0" to say the hub's plane (D177). Without a terrain everything stays at 0. The D173 flat run stays terrain-free.
3. `src/junction_game.cpp`, `GenerateJunctionNear`: replace `GenerateJunction(spec, &shifted)` with `ConstructJunctionTMap(spec, &shifted, std::nullopt, true)` converted to a `JunctionGenerateResult`, inline in the loop:
   - `layout` set only when the T map built a layout and `clean` (an unclean layout is not shown);
   - `error` = `None` when set, else `NoLayout`;
   - `hub` = the T map's `hub`; `score` = `ScoreJunctionLayout(layout, spec)` (no terrain cost yet: say so in a comment, it is a known gap); `prio_tiles` = `spec.prio_tiles`.
   - `Debug(misc, 3, "junction T map at offset {},{}: {} us, {}", d.x, d.y, micros, log)` per attempt (use the existing `Debug` form in this file).
   Update the function's doc comment (it now builds the projected T map, D173/D177). Leave `GenerateJunction`, the console command, the library tier and `junction_gui.cpp` unchanged.

## Do not
No change to the router, the old-router path, `GenerateJunction`, `FitJunctionLibrary*`, `JunctionTerrainPlanes`, the GUI, decisions, skeleton, signal pass, checker, `junction_organic.*`, savegame, network or command code. No new settings. No `std::to_string`/`std::printf` in tests; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
- Test 1's layout differs from the flat one; test 4 not at offset 0 or not equal to the direct call.
- An existing test changes outcome (name it and its failure).
- OVER BUDGET; anything not covered.

## Gates
0 warnings in both builds; the jt runs above pass. One commit on `wt-g4`: `Feature: Junction tool builds the projected T map`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence (each test, the failure seen); members added; jt lines with times; Release time of test 4's `GenerateJunctionNear` call; stops; `git status --porcelain`, `git log -1`.

When you have reported, stop.
