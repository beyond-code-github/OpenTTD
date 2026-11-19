# Brief TM5p (probe): the composer's prio rows given to the signal pass (on probe-st)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (on `wt-tracks`). Run `git switch probe-st` (at 703a0c97cf). Commit only on probe-st. Build dir `build-rel`, target `openttd_test` only. Finish with `git switch wt-tracks` and rebuild `openttd_test` in build-rel.
- Time box: 120 minutes. One heavy job at a time. pv `timeout -s KILL 900`; single runs `timeout -s KILL 120`.
- S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`.

## Why
DESIGN-tracks §16.5 (TM5) and D345, D349, D353. On probe-st, `PLANAR_COVBEND=1` builds each prio row as a strand `prio:<lane>` following its lane piece by piece (box loop, ~1960). T2/T3 solve, but end to end the signal pass fails with "the prio element at X,Y does not fit": its D345 search re-derives the row from tiles (`row_of`, `corner`: one half piece between axis pieces) and cannot follow a row through a diagonal. The archived probe `archive/probe-pr` solved this by handing the solved rows to the signal pass. Port that, with CheckPrio kept (the checker gets D353 instead of a bypass).

## Read first
- `CODINGSTYLE.md` (C++ part).
- The reference diff, read only (do not cherry-pick or merge): `git diff feature/junction-tool archive/probe-pr -- src/junction/signals src/junction/planar/planar_plan.cpp src/junction/junction_rules.cpp src/junction/planar/planar_components.h`, and in `git diff feature/junction-tool archive/probe-pr -- src/junction/planar/planar_components.cpp` the hunks touching `ExportRoutes`, `VariantOutcome::rows`, `layout.rows`. Ignore everything else there (`PLANAR_NOROW`, `PLANAR_ROWFREE`, `params.sd`, `PRIODBG`, the removal of `PlanarWastedSideways`).
- On probe-st: `ExportRoutes`, `VariantOutcome`, `PickBest`, `SolvePlanarComponents`' layout fill; `RenderPlanarRoutePrograms` (planar_plan.cpp); `SignalJunctionTrackRoutes`' deferred search (junction_signals.cpp ~350-515); `CheckPrio` (junction_rules.cpp ~1330-1400); D345 and D353 in `docs/junction-tool/DECISIONS.md`.

## Changes (all on probe-st; without `PLANAR_COVBEND` every output is exactly today's)
1. `planar_components.h`: `PlanarComponentLayout` gains `std::vector<std::vector<PlanarProgramPiece>> rows; ///< Each prio row as solved, upstream end first, ending beside its merge (PLANAR_COVBEND only).` Keep `wasted` and `PlanarWastedSideways` as they are.
2. `planar_components.cpp`: as in the reference: `VariantOutcome::rows`; `ExportRoutes` takes `rows` and fills it from every strand named `prio:...`, but only when `PLANAR_COVBEND` is set (otherwise it leaves `rows` empty); every `ExportRoutes` call passes `out.rows`; `PickBest` and `SolvePlanarComponents` copy `best->rows` into `layout.rows` (keep the `layout.wasted` lines).
3. `junction_signals.h`: `JunctionSignalNetwork::prio_rows` as in the reference.
4. `planar_plan.cpp` `RenderPlanarRoutePrograms`: the reference's block rasterising `layout.rows` into `out.network.prio_rows` (with its "prio row does not rasterise" failure).
5. `junction_signals.cpp`: the reference's changes to the deferred search, EXCEPT: keep `CheckPrio(ctx)` for given rows too (no bypass), and leave out the `PRIODBG` block. Keep `placed_merges`, `given`, `row_index`, the `row_of`/`plain`/`row_free`/`candidate` changes, the drawing of the given row, the diagonal touch point, the row signal trackdir, the "signals too far apart" judge for given rows, and the "does not reach its merge" failure.
6. `junction_rules.cpp` `CheckPrio`: the reference's D353 hunk (a row piece on the opposite diagonal half of a main-line coverage tile is not a violation), with the comment citing D353.
7. Nothing else.

## Measure
1. Gate: `tools/junction/jt` fast planar and signals tags per `docs/junction-tool/TESTS.md` (counts and times, before and after the change; any new failure listed).
2. `PLANAR_COVBEND=1` pv T2 and T3 into `<S>/tm5-pv-<hub>.txt` (should equal `<S>/cb3-pv-<hub>.txt`: report any difference).
3. End to end with `PLANAR_COVBEND=1` for T2 `o0t2f1s1` and T3 `o0t1f1s1`, then for the next two best variants of each from `<S>/cb3-pv-<hub>.txt`: `<env> PLANAR_ONLY=<tag> JUNCTION_COMPONENTS_TOPOLOGY=<hub> JUNCTION_COMPONENTS_OUT=<S>/tm5-<hub>-<tag>.json timeout -s KILL 120 build-rel/openttd_test "planar: dump a mapped component layout" -s > <S>/tm5-e2e-<hub>-<tag>.txt 2>&1`: plan result (`MAPPED` line or the failure text), `VIOLATION` lines grouped by rule and message with counts.
4. The same end to end without `PLANAR_COVBEND` for T2 and T3 (no `PLANAR_ONLY`): must still pass with 0 violations as today.

## Do not
- Touch the composer's construction, the lazy loop, slots, paths, shape, ladders. Bypass CheckPrio or CheckPrioReach. Touch ports 4077/4079 or start a game client. Use git stash. Run two heavy jobs at once.

## Commit and report
- One commit on probe-st: `Probe: Junction composer prio rows given to the signal pass (PLANAR_COVBEND)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Report: references read; commit hash; members added; results of 1-4; any stop; `git status --porcelain`; `git log -1`. Finish with `git switch wt-tracks` and the rebuild. When you have reported, stop.
