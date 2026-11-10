# P3: the flat map is computed once per spec, not once per offset (D173, PERF2)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Why
`docs/junction-tool/PERF2.md`: `ConstructJunctionTMap(spec, &terrain, nullopt, true)` first runs D173's flat map (`ConstructJunctionTMap(spec, nullptr, nullopt, false)`, 21-33 ms Release), which depends on the spec only, and `GenerateJunctionNear` repeats it at each of up to 13 offsets. It is computed once per call of `GenerateJunctionNear` instead.

## State
Worktree `/home/pete/OpenTTD/.claude/p3`, branch `wt-p3`, head given at launch (feature/junction-tool). Create `build` (Debug) and `build-rel` (Release) like the main ones (clang-18 with ccache); build `openttd_test` only. Tests only through `tools/junction/jt` with bracketed tags (TESTS.md).

## Read first (once each)
`CODINGSTYLE.md`; `ConstructJunctionTMap`'s declaration and its D173 block (how the flat result's `height`, `layout` and `hub` are used: first height, A's ports) in `src/junction_generator.{h,cpp}`; `GenerateJunctionNear` in `src/junction_game.cpp`; `PERF2.md`.

## Tests, red first
1. In `src/tests/junction_game.cpp`, `[tmapnear]`: "GenerateJunctionNear runs the flat map once": over a terrain where the anchor fails and a later offset builds (block the anchor's hub tile region as the existing "a blocked spot" style tests do, or reuse one), the result equals today's result (layout, offset, hub, score), and a counter shows the flat map ran once. The counter: `JunctionTMapResult` gains `int flat_runs = 0; ///< P3: how many times this call ran D173's flat map (0 when the caller passed it).`; `GenerateJunctionNear`'s result cannot carry it, so test through a new overload is NOT allowed; instead assert on time: the test's call takes less than the flat map's time multiplied by the number of offsets tried, measured in the test (make the assertion generous: under half of that product). Red: today it exceeds it.
   If a time assertion proves flaky in three runs, stop and report; do not loosen it further.
2. In `src/tests/junction_map.cpp`, `[junctionmap]`: "a supplied flat map gives the same T": for both specs, `ConstructJunctionTMap(spec, nullptr, nullopt, true, &flat)` equals `ConstructJunctionTMap(spec, nullptr, nullopt, true)` (layout, height, hub), and its `flat_runs` is 0 while the other's is 1.

## Changes
1. `src/junction_generator.h/.cpp`: `ConstructJunctionTMap` gains a last parameter `const JunctionTMapResult *flat = nullptr` (doc: "D173's flat map for this spec, computed once by the caller; when null and line_router is set, it is computed here (P3)"). The D173 block uses it when given (and does not count its time in `micros_flat`), else computes as today and sets `flat_runs = 1`. Everything the block derives from the flat result (first height, A's ports, the "flat map:" failure return) is unchanged.
2. `src/junction_game.cpp`, `GenerateJunctionNear`: compute the flat map once before the offset loop (`ConstructJunctionTMap(spec, nullptr, std::nullopt, false)`), and pass it to every offset's call. If the flat map builds no layout, return as the T map would have (its "flat map:" log, NoLayout) without trying offsets.

## Do not
No change to the router, the generator's routing, the GUI, decisions, savegame, network or command code. No other caching. No `std::to_string`/`std::printf` in tests; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
Any existing test changes outcome; the golden trace changes; OVER BUDGET; anything not covered.

## Gates
0 warnings; `jt fast build-rel '[tmapnear]' 5`, `jt fast build '[junctionmap]' 5`, `jt fast build '[junctionmapedges]' 5`, `jt fast build-rel '[projection]' 5`, `jt fast build-rel '[projectionedges]' 5`, `jt gate build-rel` pass. One commit on `wt-p3`: `Change: Junction hover search computes the flat map once`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
Commit; red evidence; members added; GenerateJunctionNear Release time for PERF2's rows a (flat) and d (town hill, no tunnel) before and after, measured with a temporary hidden test removed before the commit; jt lines; stops; `git status --porcelain`, `git log -1`.

When you have reported, stop.
