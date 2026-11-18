# Brief PR (probe, throwaway): the composer's prio rows go to the signal pass as real track

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`. Branch `probe-pb7` is at {HEAD}. Create `probe-pr` from it; commit only there. Finish with `git switch wt-planar` and rebuild `openttd_test` in build-rel.
- Time box: 90 minutes.
- Background: `/home/pete/OpenTTD/docs/junction-tool/STATUS.md` (the PB7 line). On probe-pb7 the bendy prio composes (T2 best 909.25 74x35, T3 best 1765.25 79x46), but the end to end fails: "the prio element at X,Y does not fit". The signal pass's prio search (`src/junction/signals/junction_signals.cpp` ~:340-510) computes its own row tile per main-line piece (`row_of`) and can only pass a single 45-degree piece between axis pieces; the near lane's S-bend has two or more diagonal pieces. The composer already solves each row as a strand `"prio:" + <lane>` (n-lane `Join`, prio case), parallel to its lane, ending level with the merge.

## Read first (once each)
- `CODINGSTYLE.md`.
- `planar_components.h`: `PlanarProgramPiece`, `PlanarComponentLayout`.
- `planar_components.cpp` on probe-pb7: the route program export (~:2440-2515, `Pieces(geo.u, geo.v, geo.L, geo.d)` per segment) and the n-lane `Join` prio rows.
- `planar_plan.cpp`: `RenderPlanarRoutePrograms` (~:190-300).
- `src/junction/signals/junction_signals.h` (`JunctionSignalNetwork`) and `junction_signals.cpp` ~:330-510 (the deferred prio search: `row_of`, `plain`, `row_free`, `candidate`, `try_leaf`, `search`).

## Changes
1. **Composer output.** Add `std::vector<std::vector<PlanarProgramPiece>> rows; ///< Each prio row as solved, upstream end first, ending beside its merge.` to `PlanarComponentLayout`. In the export, for every strand whose name starts with `"prio:"`, append its pieces in segment order exactly as route programs do (`Pieces(...)` per segment, kind `Ground`, line 0). If a row strand has no geometry entry in the export's geometry map, stop and report.
2. **Network.** Add `std::vector<std::vector<JunctionPiece>> prio_rows; ///< Rows given by the caller (planar composer), each upstream end first, its last piece beside its merge.` to `JunctionSignalNetwork`.
3. **Render.** In `RenderPlanarRoutePrograms`, after the routes, rasterise each of `layout.rows` with the same move building as a route (F/L/R per piece against the previous; no decks) and `RasterizeJunctionRoute`; check the rendered tiles equal the planned ones as for routes (on mismatch set `out.why` "prio row does not rasterise" and return). Push `rendered.pieces` to `out.network.prio_rows`. Do not add the rows to `out.layout` (the signal pass draws a row only as far as its last element).
4. **Signal pass, deferred prio search only.** After `side_of` is known for the merge piece, look for a given row: the entry of `network.prio_rows` whose last piece's tile equals the merge piece's tile plus `TileIndexDiffCByDiagDir(side_of(merge piece))`. If none, the search is unchanged. If one (`given`):
   - `row_index(m)`: for an axis piece m (`is_axis(m)`), the index i of the piece of `given` on tile `at(m)->tile + TileIndexDiffCByDiagDir(side_of(*at(m)))` whose track equals the main-line piece's track; none otherwise.
   - `row_of(m)`: for an axis piece, the tile of `given[row_index(m)]` when it has one, else none; for any other piece, none.
   - `row_free(m)`: for an axis piece, as today (a row tile, free of track, no switch on m); for any other piece, true (the given row passes it).
   - `try_leaf`: replace the per-piece row drawing (the `!is_axis(m)` branch and the `plain(m)` axis branch) by drawing every piece of `given` from index `row_index(pos.back())` to `row_index(0)` (its track on its tile); keep the element joins as today (the corner on the main-line tile and the `h2` bits on the row tile, without the axis bit, which the given row already draws).
   - Everything else (candidates, signals, `CheckPrio`/`CheckPrioReach` judgement, the violation loop, `prio_beside` for axis pieces with a row tile) unchanged.
   - If `row_index(0)` has no value, set why "the given prio row does not reach its merge" (formatted with the merge tile) and continue to the next merge.
5. Comments: one line per change, plain rule, no D-refs.

## Measure
- Build `openttd_test` in build-rel.
- End to end on T2 and T3: `JUNCTION_COMPONENTS_TOPOLOGY=<hub> JUNCTION_COMPONENTS_OUT=<S>/pr-e2e-<hub>.json build-rel/openttd_test "planar: dump a mapped component layout" -s > <S>/pr-e2e-<hub>.txt 2>&1`, S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`. Report: does it pass `REQUIRE(plan.layout.has_value())` (if not, `plan.why`); the `VIOLATION` lines grouped by rule with counts; the `MAPPED` line.
- If a hub's plan succeeds, write its signalled ASCII: add a temporary line in that test writing `RenderJunctionAscii(*plan.layout)` to `<out>.ascii` (include `junction/junction_route.h`), run it, then revert the test file before committing.
- `tools/junction/jt fast build-rel '[planar]'` and `tools/junction/jt fast build-rel '[junctionsignals]'` if that tag exists (grep `TEST_CASE` tags in `src/tests/junction/signals/`): list failures only.

## Do not
- Change the straight prio pattern, `CheckPrio`, `CheckPrioReach`, the composer's geometry, or anything for merges without a given row.
- Touch ports 4077 and 4079, or start a game client. Use git stash. Run two heavy jobs at once.

## Commit and report
- Commit on probe-pr: `Probe: Junction composer prio rows drawn by the signal pass (throwaway)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Write `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-PR.md` (do not commit): changes, end-to-end per hub, violations, test failures.
- Reply with: per hub plan result and violations, the ASCII paths, test failures, commit hash, `git status --porcelain`.

When you have reported, stop.
