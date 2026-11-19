# Brief CM (probe, throwaway): face slack as a linear compactness term

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`. Run `git switch -c probe-cm probe-pr` (probe-pr is at {HEAD}); commit only there. Finish with `git switch wt-tracks` and rebuild `openttd_test` in build-rel.
- Time box: 90 minutes. Single-variant runs `timeout -s KILL 30`; pv runs `timeout -s KILL 600`; end-to-end `timeout -s KILL 60`. One run at a time.
- S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`.

## Why
The engineer sees large empty areas inside the T2 hub (between a line's walls and the facing lines). The objective (outline area plus track length) cannot see space the outline encloses anyway. The ladder rungs already measure, per face, the distance from each wall end point to the facing wall (`RungCon`: distance >= k). Charging that distance makes every face want to be thin: a linear measure of enclosed empty space.

## Read first
- `src/junction/planar/planar_components.cpp` on probe-pr: `RungSpec`, `RungCon`, `LadderRungs` (~2580-2700), the ladder pass in `SolveMappedVariant` (~2810-2850), `SolveLazy` (~2340-2380, `obj_extra` is cleared there and replaced by `LineariseArea`).
- `src/junction/planar/planar_model.h`: `obj_extra` and where `Solve` adds it to the costs (~554-562).

## Changes
1. `planar_model.h`: add `std::map<uint32_t, double> obj_fixed; ///< Objective terms kept through the area step (face slack).` next to `obj_extra`; in `Solve` the cost of variable i is `cost[i] + obj_extra[i] + obj_fixed[i]` (each 0 when absent). Nothing clears `obj_fixed`.
2. `planar_components.cpp`: split `RungCon` into `Expr RungExpr(const Model &M, const RungSpec &r)` (the signed distance, the left-hand side today: `Dot(Right(h), ts.a) - Dot(Right(h), p)` for sign > 0, the negation otherwise) and `RungCon` returning `Ge(RungExpr(M, r), r.k)`. Use the existing expression type of the file; if `Ge` cannot take an expression built that way, stop and report.
3. In the ladder pass, when `PLANAR_FACEW` is set (a double w, parsed with `std::from_chars`), after adding the rung rows: for every rung, for each (var, coef) term of `RungExpr(next->M, r)`, `next->M.obj_fixed[var] += w * coef`. The constant part is dropped.
4. Under `PLANAR_TIMES`, after the final solve of each variant, print `slack <S> rungs <N>` to stderr: S = the sum over the last pass's rungs of (`RungExpr` evaluated on the accepted solution - k), divided by 2 (tiles), N = the rung count. When the ladder did not run, print `slack - rungs 0`.

## Measure
Variant run: `PLANAR_ONLY=o1t3f1s0 PLANAR_TIMES=1 <env> timeout -s KILL 30 build-rel/openttd_test "planar components: T2 from its topology text (mapped) at the probe's area" -s > <S>/cm-<name>.txt 2>&1`: area, size, ms, track from the `variant o1t3f1s0 area ...` line, slack and rungs from the last `slack` line.
1. No env: must be 932.25 75x31; record slack.
2. `PLANAR_FACEW` = 1, 3, 10, 30: area, size, track, slack, ms, or failure.
3. `pv run build-rel T2` (tool `/home/pete/OpenTTD/tools/junction/pv`, `--out <S>/cm-pv-T2-<w>.txt`) with no env and with `PLANAR_FACEW` = 3 and 10: built and failed counts, best variant and area, per variant area (table) for both.
4. End to end on T2 and T3 with `PLANAR_FACEW=10`: `JUNCTION_COMPONENTS_TOPOLOGY=<hub> JUNCTION_COMPONENTS_OUT=<S>/cm-<hub>.json timeout -s KILL 60 build-rel/openttd_test "planar: dump a mapped component layout" -s > <S>/cm-e2e-<hub>.txt 2>&1`: plan success or its message, the `MAPPED` line, `VIOLATION` lines grouped by rule with counts.

## Do not
- Change the rungs' rows, `LadderRungs`, the area step, the clash loop, the slot layer, the signal pass or the checker.
- Touch ports 4077/4079 or start a game client. Use git stash. Run two heavy jobs at once.

## Commit and report
- Commit on probe-cm: `Probe: Junction face slack charged in the objective (throwaway)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Write `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-CM.md` (do not commit): changes and measurements 1-4.
- Reply with the measurements, the commit hash, `git status --porcelain`.

When you have reported, stop.
