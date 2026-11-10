# G3a: the line-router T starts at the flat map's height (D173)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Principle (D173)
Flat empty ground is the relaxation of every terrain, so the height at which the T map builds on flat ground with the old router is an exact lower bound for the line router. The line router starts there; it routes each edge once on flat ground (no failed heights).

## State
- Worktree `/home/pete/OpenTTD/.claude/g3a`, branch `wt-g3a`, head given at launch (feature/junction-tool, P2 merged: `ConstructJunctionTMap(spec, terrain, first_height, line_router = false)`, `line_router_trace`, the golden test "line router results are unchanged by the speed work"). Create `build` (Debug) like `/home/pete/OpenTTD/build` and `build-rel` (Release) like `/home/pete/OpenTTD/build-rel`; build `openttd_test` only.
- D173 in `docs/junction-tool/DECISIONS.md` (main checkout).

## Test speed
`[junctionmap]` under its current Debug time (about 11 s; report it at the start) and must not grow. Develop against it.

## Read first (once each)
`CODINGSTYLE.md`; `ConstructJunctionTMap` (whole) and its declaration; `src/tests/junction_map.cpp`.

## Change
In `ConstructJunctionTMap`, when `line_router` is true and `first_height` is unset: first run `ConstructJunctionTMap(spec, nullptr, std::nullopt, false)` (flat, old router). If it built no layout, return its result with `log` prefixed by `"flat map: "` on each line (nothing to project). Otherwise the line router's heights start at its `height`. Add `int64_t micros_flat = 0; ///< D173: the flat map that gives the line router its first height.` to `JunctionTMapResult`, included in `micros`. Comment quoting D173. With `terrain` set the same rule applies (the flat map ignores the terrain by design; this step does not test terrain).

## Tests (red first)
1. "the line-router T starts at the flat height": for both specs, `ConstructJunctionTMap(spec, nullptr, std::nullopt, true)`: `heights_tried == 1`, `height` equals the old router's height, `clean`, and the layout equals the old router's layout tile for tile (tracks, bridges, signals). Red: `heights_tried` is 3 today.
2. The golden test changes legitimately: its expected trace becomes only the lines of the built height (the last lines of today's expected trace, unchanged); assert that the new trace equals exactly that tail of the old expected strings (keep the old strings in the test and compare with the tail, so the identity is visible).
3. BENCH (Release, report): both specs, line router: `micros`, `micros_flat`, `micros_route`; old router `micros`. Target < 100 ms Release for the line router T.

## Do not
No change to the router, the old router, decisions, windows, fences, skeleton, finish, signal pass, checker, `GenerateJunction`, `junction_organic.*`, savegame, network or command code. Do not change the default of `line_router`. No `std::to_string`/`std::printf` in tests; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
- The line router does not build at the flat height for a spec, or its layout differs from the old router's.
- `[junctionmap]` grows.

## Gates
0 warnings; `[junctionmap]` green; once at the end `"[junction]~[slow]~[mapexport]"` green in Release (report time). One commit on `wt-g3a`: `Change: Junction line-router T starts at the flat map's height`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence; members added; BENCH lines (Release); suite results with times; stops; `git status --porcelain`, `git log -1`.

When you have reported, stop.
