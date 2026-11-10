# E3b: span edges are checked against the terrain (D175, D178)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Why
E3 routes B's doubled bridges as span edges fixed by their nodes: the request's `prefix` is `B<n>` and the goal is the far head. `JunctionLineRouteRequest::prefix` is replayed without any check ("not re-checked against terrain or obstacles", D150: the painter's frozen moves are track already laid). So a house under a bridge head or a span tile no longer stops the layout, and the game would fail to build it. The router gets an opt-in: prefix pieces checked exactly like searched pieces.

## State
- Worktree `/home/pete/OpenTTD/.claude/e3b`, branch `wt-e3b`, head given at launch (feature/junction-tool with E3 merged). Create `build` (Debug) and `build-rel` (Release) like the main ones; build `openttd_test` only. If a build fails because a precompiled header is older than a system header, delete that .pch and rebuild.
- Tests only through `tools/junction/jt` (read `docs/junction-tool/TESTS.md`): `tools/junction/jt fast build [checkedprefix] 5` (new tag), `tools/junction/jt fast build-rel [linerouter] 5`, `tools/junction/jt fast build-rel [projection] 5`, `tools/junction/jt fast build [junctionmap] 5`, final `tools/junction/jt gate build-rel`. OVER BUDGET is a stop. Never type tag filters by hand.

## Read first (once each)
`CODINGSTYLE.md`; D150, D175 in `docs/junction-tool/DECISIONS.md`; `JunctionLineRouteRequest::prefix` in `src/junction_line_router.h`; in `src/junction_line_router.cpp` where the prefix is replayed and how a searched piece and a searched bridge are checked (`PieceUsable`, the bridge move's head and span checks, `SpanOk`, `blocked`, `existing`); `route_line`'s `prefix` parameter and the span edges in `ConstructJunctionTMap` (`src/junction_generator.cpp`).

## Tests, red first
In `src/tests/junction_line_router.cpp`, tag `[junction][checkedprefix]`:
1. "a checked prefix bridge over a house is refused": a one-track request on a flat test terrain, prefix `B3` from an axis start, goal the far head, a `Blocked` tile under the second middle tile, `check_prefix = true`: no route. The same with `check_prefix = false`: routes (today's behaviour). Red: the flag does not exist (declare it first, unused, so the red is an assertion).
2. "a checked prefix bridge head on a house is refused": as 1 with the far head's tile `Blocked`: no route with the flag.
3. "a checked prefix that is legal routes as before": as 1 with no house: routes, same pieces as without the flag.
In `src/tests/junction_map.cpp`, `[projection]`:
4. "a house under B's doubled bridge is reported": the test terrain with one middle tile of lane B's bridge (from the flat T's layout, as E3's tests pick tiles) `Blocked`; restrict the terrain's rows as E3's tests do (`LimitMapTestRows`). No layout or a clean layout that does not bridge over it; the log names a B lane edge. Red: today a layout bridges over the house.

## Changes
1. `src/junction_line_router.h`: `JunctionLineRouteRequest` gains `bool check_prefix = false; ///< When set, the prefix's pieces, bridges and tunnels are checked against the terrain, blocked and existing like searched ones (E3b: span edges fixed by their nodes); unset, the prefix is the painter's frozen track (D150).`
2. `src/junction_line_router.cpp`: when `check_prefix` is set, apply to each replayed prefix piece and bridge the same checks the search applies to a piece or bridge move it generates (usable at its height, not blocked, not clashing with existing; for a bridge: heads usable, every middle tile passes the span check, no tunnel crossing rule broken); any failure ends the request with no route and the error text the search would give. Reuse the search's check functions; do not duplicate their logic.
3. `src/junction_generator.cpp`: the span edges pass `check_prefix = true` (through `route_line`'s prefix path; one line, comment citing this brief).

## Do not
No other router change; no change to the painter, the old router, the generator beyond change 3, decisions, savegame, network or command code. No `std::to_string`/`std::printf` in tests; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
- Any existing test, the golden trace or the flat identity changes.
- The search's checks cannot be applied to a replayed piece without duplicating them: report where they live and what they need.
- OVER BUDGET; anything not covered.

## Gates
0 warnings in both builds; the jt runs above pass. One commit on `wt-e3b`: `Fix: Junction span edges are checked against the terrain`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence; members added; test 4 outcome with the log; jt lines with times; stops; `git status --porcelain`, `git log -1`.

When you have reported, stop.
