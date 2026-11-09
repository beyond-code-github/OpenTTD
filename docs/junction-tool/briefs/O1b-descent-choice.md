# O1b: a ramp is a choice beside a foundation (D152 amended)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/o1b`, branch `wt-o1b`, head given in your launch prompt (it contains O1, 5634729668). You are the only writer there; never touch sibling worktrees or the main worktree.
- Build: configure once as in `briefs/O1-router-heights.md`, then `cmake --build build-rel --target openttd_test -j 8 > build.log 2>&1`.
- Tests: `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/ptest.sh build-rel/openttd_test "<filter>" 8`; `"[junction]~[slow]"` then `"~[.]"` at the gate. A lone segfault/SIGILL under load is the machine: rerun alone and report.
- Decisions: DECISIONS rows D152 and its amendment D152a (read first).

## Why (D152a)
O1's height step tries level first. With `build_on_slopes` true (the game default), a tile that slopes down along the track is also usable level at its z_max (on a foundation), so the router never takes the descending ramp and can't go down. Both are buildable; the cost must choose.

## Read first
- DECISIONS D152, D152a.
- `src/junction_line_router.cpp` (whole; `HeightStep`, `PieceUsable`, the F/L/R loop) and the O1 tests in `src/tests/junction_line_router.cpp` (`SetUpRamps`, `SetUpValley`, the six O1 cases).

## Tests (red first)
1. "FindJunctionLineRoute: descends with build_on_slopes": test 2 of O1 ("descends into a valley") with `terrain.build_on_slopes = true`: route found. Red: "no route".
2. "FindJunctionLineRoute: ramp or foundation by price": with `build_on_slopes = true`, a single descending tile between two flat tiles at the upper height on one side and a route that may continue either at the upper height (a flat row at z) or at the lower one (a flat row at z - 1), each leading to its own goal piece of equal distance; with `foundation_cost` high the route takes the ramp, with `slope_piece` high it stays level. Build the terrain so that only these two continuations exist; if you can't make both reachable with equal track length, stop and report the layout you tried.
3. The O1 `[slow]` informed = uninformed case extended with test 1's request: equal costs.

## Changes
`junction_line_router.cpp`, the F move only: compute up to two successors. Variant A (today): each track takes level when usable at z, else a ramp. Variant B: each track takes a matching ramp (up or down, the O1 rule) when its tile offers one, else level. Push A; push B only if some track's choice differs from A. L/R moves unchanged (level only). Nothing else changes.

## Do not
- Change the height rules, costs, bridges, the heuristic or any other move.

## Stop conditions
- Any existing test fails (beyond a lone hardware flake); test 3's costs differ; test 2 can't be built as described.

## Gates
1. Build: no errors, no new warnings. 2. `"[junction]~[slow]"`, then `"~[.]"`. 3. Commit: `Fix: Junction line router may descend a slope where a foundation would also fit`, body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
References read; commit; red evidence per test; names added; suite results; stops; `git status --porcelain` and `git log -1`. When you have reported, stop.
