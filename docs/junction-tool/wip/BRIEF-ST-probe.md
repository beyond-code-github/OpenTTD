# Brief ST (probe): strip every unruled row family and segment bound from the mapped composer

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (on `wt-tracks`). Run `git switch -c probe-st feature/junction-tool` (at HEAD_HASH). Commit only there. Build dir `build-rel`, target `openttd_test` only. Finish with `git switch wt-tracks` and rebuild `openttd_test` in build-rel.
- Time box: 120 minutes. One heavy job at a time. pv runs `timeout -s KILL 900`; end to end `timeout -s KILL 60`.
- S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`.

## Why
DESIGN-tracks §16.4 (read it, and §16.1). The question: with the headings unchanged, does the length solve find layouts at least as compact as today's, within 15 s per variant, once every constraint with no ruling behind it is gone? The rules audit (`docs/junction-tool/wip/AUDIT-rules.md`) traced each constraint; this probe removes the unruled row families and bounds at once, behind one environment switch.

## Read first
- `CODINGSTYLE.md` (C++ part).
- `src/junction/planar/planar_model.h`: `TagScope` and `g_tag`, `Model::Add`, `Model::AddSeg`.
- `src/junction/planar/planar_components.cpp`: `MappedComposer::Double` (~986), `Bundle`'s doubled piece (~1200-1210), the `TagScope`s named below.

## Change (behind `PLANAR_STRIP=1`; without it the code is exactly today's)
1. `planar_model.h`: a thread-local `bool g_strict = false;` beside `g_tag`, and a struct `StrictScope` that sets it true for its lifetime and restores the old value (same pattern as `TagScope`). A file-local helper `bool PlanarStrip()` returning whether `PLANAR_STRIP` is set (read once into a static).
2. `Model::Add`: when `PlanarStrip()` and the row's tag starts with any of `fork gap`, `fray gap`, `flex`, `bundle offset`, `run offset`, `fan rule`, `side rule`, do not add the row (return before the constant check).
3. `Model::AddSeg`: when `PlanarStrip()` and not `g_strict`: `lo = std::min(lo, IsAxis(d) ? 2 : 1)` and `hi = std::nullopt`, applied at the top, before anything uses them (the optional-segment rows then use the new values; `OPTMAX` stays as their cap).
4. `StrictScope` (one each) around the two parallelogram diagonals in `MappedComposer::Double` (`M.AddSeg(l, Turn(h, sg), 2, 2)` and `M.AddSeg(c, Turn(h, sg), 2, 2)`) and around the same two lines in `Bundle`'s doubled piece (`l` and `cpn`). Nothing else is strict (the step back after a doubled stretch is stripped).
5. Nothing else. If a strand comes out disconnected or a construction check fails (`M.error`), record the text and continue with the measurements.

## Measure (with `PLANAR_STRIP=1`; baselines are the main line at 2b49a2b6c0, files `<S>/lo1-pv-<hub>-after.txt`)
1. `tools/junction/pv run build-rel T2 --out <S>/st-pv-T2.txt`, then T3, then LL2 (each `timeout -s KILL 900`; record "timed out" and go on).
2. Per hub, against the baseline: variants built before/after; best variant with area, WxH, track, wasted, ms before/after; max ms after; failure reasons grouped with counts; variants that built before and fail after, listed with reasons.
3. For the T2 best and the T3 best after: `PLANAR_STRIP=1 PLANAR_ONLY=<tag> JUNCTION_COMPONENTS_TOPOLOGY=<hub> JUNCTION_COMPONENTS_OUT=<S>/st-<hub>.json timeout -s KILL 60 build-rel/openttd_test "planar: dump a mapped component layout" -s > <S>/st-e2e-<hub>.txt 2>&1`: plan result, `MAPPED`, `VIOLATION` lines grouped by rule with counts.
4. If T2 builds fewer than half its baseline variants: for the first failing T2 variant whose reason is infeasible, rerun it alone with `PLANAR_IIS=1` (`timeout -s KILL 60`) and report the row tags of the IIS grouped by family.

## Do not
- Touch the slot layer, paths, shape, the lazy loop, the ladders, the signal pass or the checker. Strip any other family or bound than those listed. Touch ports 4077/4079 or start a game client. Use git stash. Run two heavy jobs at once.

## Commit and report
- One commit on probe-st: `Probe: Junction strip unruled composer rows and bounds (PLANAR_STRIP)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Report: references read; commit hash; members added; the tables of 2; e2e results of 3; the IIS of 4 if run; any stop; `git status --porcelain`; `git log -1`. Finish with `git switch wt-tracks` and the rebuild. When you have reported, stop.
