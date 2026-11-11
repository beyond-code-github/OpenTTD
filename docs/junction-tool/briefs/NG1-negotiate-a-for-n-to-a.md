# NG1: when N->A fails against A, A is routed again clear of N->A (D181)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Why
`docs/junction-tool/DIAG-house.md`: with a house at (xI-1, 0) on main line A, A routes first and detours on row 1 from x 38 to 53 (equally cheap as climbing back right after the house; nothing tells A about N->A). N->A must merge onto A's straight X piece at x >= n_to_a_min_x (52); only (52,1), right before A's climb, and (54,0), behind A's bend, are left, and N->A fails at every height (33 heights, ~12 s per spot, 40 s in the tool). No tile blocks N->A and the D172 window is not the cause. D181: negotiate: route N->A without A, reserve its tiles, route A again around them, route the height again.

## State
Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-ng1`, head given at launch (feature/junction-tool). Create `build` (Debug) and `build-rel` (Release) like the main ones (clang-18 with ccache); build `openttd_test` only. Tests only through `tools/junction/jt` with bracketed tags (see `docs/junction-tool/TESTS.md`): `'[projectionedges]'` and `'[projection]'` and `'[tmapnear]'` in Release; `'[junctionmap]'`, `'[junctionmapedges]'` in Debug; `jt gate build-rel`.

## Read first (once each, offset/limit)
`CODINGSTYLE.md`; D170a, D172, D179a, D181 in `docs/junction-tool/DECISIONS.md`; `docs/junction-tool/DIAG-house.md`; in `src/junction_generator.cpp`: `route_line` (line ~944, note it adds the raster to the layout passed AND pushes it onto `routed_rasters`, whose rasters CS2's `chained_continuation` reads for a goal's continuation), the height loop and `redo_height` (~1149), A's request (~1320), A->S, the D172 window and N->A (~1340-1420), the ports-widened redo (~1700); the test "an obstacle on the main line beside the hub gives no layout" in `src/tests/junction_map.cpp` (~556).

## Tests, red first
1. `src/tests/junction_map.cpp`: replace the test above with "an obstacle on the main line beside the hub: A is routed again clear of N->A (D181)", tags `[junction][projectionedges]` (no `[slow]`). Same setup (house at (xI-1, 0), same REQUIREs). Asserts: `result.layout.has_value()`; `result.clean` (INFO the violations); `result.heights_tried == 1`; `result.log` contains `"height <flat.height>: N->A negotiates: A routed again clear of N->A\n"`. Update the comment above it (it no longer documents the growth stop). Red: today no layout (assertion failure; record the message).
2. Unchanged: every other building test, the flat identity and the golden traces (negotiation only runs after an N->A failure, which never happens on flat ground).

## Changes (`src/junction_generator.cpp`, ConstructJunctionTMap, line-router path only)
1. Before the height loop, next to `redo_height`: `std::vector<TileIndexDiffC> a_reserved; ///< D181: N->A's tiles (off row 0) that A is routed clear of at a_reserved_height.` and `int a_reserved_height = -1;`.
2. A's route: `route_line(pass1, a_request, a_reserved_height == height ? a_reserved : std::vector<TileIndexDiffC>{}, false)`. If A fails while `a_reserved_height == height`, the existing "A failed" path runs unchanged.
3. After `say("N->A failed")` and BEFORE `growth_cannot_help("N->A", ...)`: when `line_router && a_reserved_height != height`:
   - Build `JunctionLayout probe_layout` from `base_layout(probe_layout)` plus `AddRasterToLayout` of `*s_port_raster`, `*n_port_raster` (each when it has a value) and `*a_to_s`. Not A.
   - Save `routed_rasters` to a local copy, set `routed_rasters` to exactly those same rasters (S port, N port, A->S), so CS2's continuation does not read A's raster.
   - `JunctionRouteRequest probe = request;` (the N->A request just failed) with `probe.goals` = `{tile(x, 0), Trackdir::X_SW}` for every x in `[n_to_a_min_x, std::min(frame.width - 3, x_e)]`. `probe_blocked` = `n_to_a_blocked` plus `tile(x, 0)` for every x in `[0, frame.width - 1]` outside that goal range (so N->A arrives from below, never runs along A's row).
   - `const std::optional<JunctionRaster> probe_route = route_line(probe_layout, probe, probe_blocked, true);` (timed with `timed_route` like the others).
   - Restore `routed_rasters` from the saved copy.
   - If `probe_route` has a value: `a_reserved` = the tiles of its pieces with `tile.y > 0`; `a_reserved_height = height`; `say("N->A negotiates: A routed again clear of N->A");` `redo_height = true; height--; continue;` (same redo as the ports-widened path: the height is not counted again). Otherwise fall through to the existing growth_cannot_help line.
   Write one comment block over this quoting D181 and naming DIAG-house.md.
4. Nothing else changes: A->S is routed again on the new A by the redo; N->A is routed again normally (its goals from the new A's plain pieces).

## Do not
No negotiation for A->S, B's lanes or any other edge; no second round; no change to the router, the D172 window, growth_cannot_help, the checker, the old router, savegame, network or command code. No `std::to_string`/`std::printf` in tests; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
The new test is still red after the change (report the log lines for the redone height: A's result, N->A's result); A fails with the reservation; any other test, flat identity or golden trace changes; OVER BUDGET; anything not covered.

## Gates
0 warnings; the jt runs above pass; report `[projectionedges]` Release time (was 2.6 s) and the new test's own time. One commit on `wt-ng1`: `Change: Junction A is routed again clear of N->A when N->A fails (D181)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence; members/locals added; the new test's log; jt lines with times; stops; `git status --porcelain`, `git log -1`.

When you have reported, stop.
