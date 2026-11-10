# E3a: the line router may close an S-bend with a bridge head (D110)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Why
D110 (engineer): a lane's S-bend may end directly at its bridge's near head (zero plain tiles between them). The rasterizer supports it (`RasterizeJunctionRoute` with a `B<n>` after a half piece; `BuildParallelogramLanes` builds lane B as `RF` then `B<n>`, its "q = -1" form). The line router does not: `ExtendJunctionLineBridge` (`src/junction_line.cpp`, about line 247) rejects any non-axis last piece with "must start from axis pieces". E3 (B's lanes routed between their nodes) needs the router to produce lane B's bridge exactly as the template does.

## State
- Worktree `/home/pete/OpenTTD/.claude/e3a`, branch `wt-e3a`, head given at launch (feature/junction-tool). Create `build` (Debug) and `build-rel` (Release) like the main ones; build `openttd_test` only. If a build fails because a precompiled header is older than a system header, delete that .pch and rebuild.
- Tests only through `tools/junction/jt` (read `docs/junction-tool/TESTS.md`): `tools/junction/jt fast build [d110bridge] 5` (new tag), `tools/junction/jt fast build-rel [linerouter] 5`, `tools/junction/jt fast build-rel [projection] 5`, `tools/junction/jt fast build [junctionmap] 5`, final `tools/junction/jt gate build-rel`. OVER BUDGET is a stop. Never type tag filters by hand.

## Read first (once each)
`CODINGSTYLE.md`; D110 and D149 in `docs/junction-tool/DECISIONS.md`; `ExtendJunctionLineBridge` and its callers in `src/junction_line.cpp` and `src/junction_line_router.cpp` (the bridge move, about lines 856-868, and whatever gates it to axis pieces); in `src/junction_route.cpp` how `RasterizeJunctionRoute` places the near head after a half piece (the D110 case); `BuildParallelogramLanes` in `src/junction_generator.cpp` (lane B's `q = -1` form).

## Tests, red first (`src/tests/junction_line_router.cpp`, tag `[junction][d110bridge]`)
1. "FindJunctionLineRoute closes an S-bend with a bridge head (D110)": a one-track request on flat ground whose only route steps one row sideways and immediately bridges over existing track placed so that the template `RF` + `B<n>` fits and `RFL` + `B<n>` does not (for example existing track on the tile the `L` would need, and on the bridge's middle tiles; build the expected raster with `RasterizeJunctionRoute` from the same start and moves `RFB<n>`, and compare pieces and bridges). Routes; the route equals that raster. Red: today no route (record the error).
2. A multi-track line (two tracks) keeps today's rule: a bridge only from axis pieces (an existing multi-track bridge test covers it, or add one that expects the half-piece bridge to be refused).

## Change
In `ExtendJunctionLineBridge`: when `line.tracks == 1`, a non-axis last piece is accepted and the rasterizer decides whether the bridge is legal (its error is returned as today). For more than one track the axis rule stays (comment: D149, tracks of a multi-track line bridge together from axis pieces; D110 is a single-lane rule). If the router's bridge move has its own axis gate before calling it, relax it the same way, for one track only. Doc comment on the function cites D110.

## Do not
No other router change, no change to `RasterizeJunctionRoute`, the generator, the old router, decisions, savegame, network or command code. No `std::to_string`/`std::printf` in tests; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
- The golden trace test ("line router results are unchanged by the speed work"), the flat identity (`[junctionmap]`), or any existing test changes outcome or result: report which and how (the new move may make some route cheaper; that is a finding for the orchestrator, not something to suppress).
- OVER BUDGET; anything not covered.

## Gates
0 warnings in both builds; the jt runs above pass. One commit on `wt-e3a`: `Change: Junction line router closes an S-bend with a bridge head (D110)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence; jt lines with times; stops; `git status --porcelain`, `git log -1`.

When you have reported, stop.
