# SC1: the tool's score includes the terrain cost of the projected T map

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Why
`GenerateJunctionNear` (G4) converts the T map's result with `score = ScoreJunctionLayout(layout, spec)` and a comment saying the terrain cost is missing. The tool compares that score with the library fit's (which includes clearing and foundations), so on rough ground it can prefer a placement that costs more to build.

## State
Worktree `/home/pete/OpenTTD/.claude/sc1`, branch `wt-sc1`, head given at launch (feature/junction-tool). Create `build` (Debug) and `build-rel` (Release) like the main ones (clang-18 with ccache, as their CMakeCache shows); build `openttd_test` only. Tests only through `tools/junction/jt` with bracketed tags (`docs/junction-tool/TESTS.md`).

## Read first (once each)
`CODINGSTYLE.md`; `JunctionTerrain` (`CostAt`, `clear_cost`, `foundation_cost`, `incline`) in `src/junction_terrain.h`; `ConstructJunctionTMap` (the end of the line-router path: `FinishJunctionCandidate`, `result.hub`, frame coordinates) in `src/junction_generator.cpp`; `GenerateJunctionNear` in `src/junction_game.cpp`; `FitJunctionLibrary` (how the library fit counts terrain cost) in `src/junction_generator.cpp`.

## Tests, red first
In `src/tests/junction_map.cpp`, tag `[junction][projectionedges]`:
1. "the T map reports its terrain cost": the flat test terrain with `clear_cost = 30` on every tile: `terrain_cost` equals 30 times the number of distinct tiles holding track or a bridge head in the layout. Red: the member does not exist (declare it first, zero, so the red is an assertion).
2. "a foundation is charged, a ramp is not": the G3c ridge terrain (`MakeRidgeTerrain`, `foundation_cost` 250 as the helper sets or set it): `terrain_cost` equals the sum over those tiles of `clear_cost` plus `foundation_cost` for each sloped tile whose track is not exactly one straight track along the tile's incline. Compute the expected value in the test from the layout and the terrain with the same rule written out plainly.
In `src/tests/junction_game.cpp`, `[tmapnear]`:
3. "GenerateJunctionNear's score includes the terrain cost": over the ridge (the existing ridge test's terrain), `near.result.score.second == ScoreJunctionLayout(layout, spec).second + tmap.terrain_cost`. Red: today without the terrain cost.

## Changes
1. `src/junction_generator.h`: `JunctionTMapResult` gains `Money terrain_cost = 0; ///< SC1: clearing plus foundations of every tile with track or a bridge head; a straight track along an inclined tile is a ramp and has no foundation.`
2. `src/junction_generator.cpp`, `ConstructJunctionTMap`: when a layout is built and `terrain` is set, compute `terrain_cost` over the finished layout's tiles in frame coordinates (layout + (`frame.hub_x - result.hub.x`, `-result.hub.y`)), each distinct tile once: `clear_cost`, plus `foundation_cost` when the tile is not flat (`z_min != z_max`) unless its tracks are exactly one of X or Y and `incline` lies along that axis. Comment the rule.
3. `src/junction_game.cpp`, `GenerateJunctionNear`: `score.second` adds `terrain_cost`; replace the "known gap" comment.

## Do not
No change to the router, `FinishJunctionCandidate`, `FitJunctionLibrary`, the GUI, decisions, savegame, network or command code. No `std::to_string`/`std::printf` in tests; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
Any existing test changes outcome; OVER BUDGET; anything not covered.

## Gates
0 warnings; `jt fast build-rel '[projectionedges]' 5`, `jt fast build-rel '[tmapnear]' 5`, `jt fast build '[junctionmap]' 5`, `jt gate build-rel` pass. One commit on `wt-sc1`: `Fix: Junction tool scores the projected T map with its terrain cost`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence; members added; terrain_cost of the ridge test; jt lines; stops; `git status --porcelain`, `git log -1`.

When you have reported, stop.
