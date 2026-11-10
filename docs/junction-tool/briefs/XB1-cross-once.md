# XB1: an edge crosses a crossing node once (D171, D172)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Why
The map says A->S crosses B (the X_B crossing node, D172) once. The line router sees B's rows as empty ground when A->S is routed (B is routed later), so in "a field of obstacles grows the box" A->S found, at heights 13 and 14, a route that goes down to row 9, back UP to row 1 and down again, crossing B's rows (W row 2, b row 3) three times; the D172 width check then rejected it after a 45-56k-expansion search (STATUS, 2026-11-11 probe). A route that may cross a band of rows only once removes those routes from the search.

## State
Worktree `/home/pete/OpenTTD/.claude/xb1`, branch `wt-xb1`, head given at launch (feature/junction-tool). Create `build` (Debug) and `build-rel` (Release) like the main ones (clang-18 with ccache); build `openttd_test` only. Tests only through `tools/junction/jt` with bracketed tags: `'[crossonce]'` (new) in Debug; `'[linerouter]'`, `'[projection]'`, `'[projectionedges]'` in Release; `'[junctionmap]'`, `'[junctionmapedges]'` in Debug; gate.

## Read first (once each)
`CODINGSTYLE.md`; D171, D172 in `docs/junction-tool/DECISIONS.md`; `JunctionLineRouteRequest` in `src/junction_line_router.h`; in `src/junction_line_router.cpp` the node state, its pruning key (D161) and how a successor's pieces are checked; A->S's and N->A's requests in `ConstructJunctionTMap` (`src/junction_generator.cpp`); the test "a field of obstacles grows the box" in `src/tests/junction_map.cpp`.

## Tests, red first
1. `src/tests/junction_line_router.cpp`, `[junction][crossonce]`: "a route crosses a once-band only once": a one-track request on flat ground from row 0 to a goal on row 8, `cross_once = {2, 3}`, with obstacles placed so the cheapest route goes down past row 3, back up across rows 3..2 and down again (or, if that is hard to force, assert on a request whose only route re-crosses: no route). With `cross_once` the route never has a piece on rows 2..3 after a piece below row 3. Red: the member does not exist (declare it, unused, so the red is an assertion).
2. `src/tests/junction_map.cpp`, `[projection]`: "a field of obstacles grows the box" additionally asserts that no height's log says "A->S crossing wider than the doubled bridge" and reports its Release time (expected far below the 435 ms measured before). Red: today the log has that line at heights 13 and 14.
3. Every building test, the flat identity and the golden trace unchanged (on flat ground A->S crosses once already).

## Changes
1. `src/junction_line_router.h`: `JunctionLineRouteRequest` gains `std::optional<std::pair<int16_t, int16_t>> cross_once; ///< XB1: rows [first, last] the route may cross only once: after it has a piece beyond last (larger y), no later piece may lie on these rows. A crossing node's band (D172).`
2. `src/junction_line_router.cpp`: a node carries a flag "left the band"; set when a piece lies below `last`; a successor with a piece on the band while the flag is set is discarded. The flag is part of the pruning key (two nodes on the same tile, one past the band and one not, are different states). Bridges: a bridge's middle tiles over the band count as being on it.
3. `src/junction_generator.cpp`: A->S's request (line-router path) sets `cross_once = {w_row, b_row}`. N->A crosses the band going up: its request sets nothing here (state why in a comment: the band is crossed upwards and N->A starts below it; mirror support is not needed for this step).

## Do not
No change to the D172 window check (it stays as the judge), the checker, the old router, decisions, savegame, network or command code. No `std::to_string`/`std::printf` in tests; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
Any building test, the flat identity or the golden trace changes; OVER BUDGET; anything not covered.

## Gates
0 warnings; the jt runs above pass. One commit on `wt-xb1`: `Change: Junction A->S crosses B's rows once`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
Commit; red evidence; members added; the field test's log and Release time; jt lines; stops; `git status --porcelain`, `git log -1`.

When you have reported, stop.
