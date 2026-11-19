# Brief ST2 (probe): prio rows take sides in the lazy loop (on probe-st)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, currently on `probe-st` at 222b70302e (the ST probe: `PLANAR_STRIP`). Commit only on probe-st. Build dir `build-rel`, target `openttd_test` only. Finish with `git switch wt-tracks` and rebuild `openttd_test` in build-rel.
- Time box: 90 minutes. One heavy job at a time. pv runs `timeout -s KILL 900`; single runs `timeout -s KILL 60`.
- S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`.

## Why
ST (wip/BRIEF-ST-probe.md) failed every T2 variant with "a clash is not separable". The diagnosis on T2 o1t3f1s0 (`<S>/st-dbg-T2.txt`): lane E2's own segments clash with its prio row strand `prio:E2`, and `PlanarLineSides::LineOfStrand("prio:E2")` maps to no line, so the lazy loop has no side for the row and falls back to trial, which picks a wrong direction. D349 rules the row's side (beside its lane, away from the joiners: `PrioRow::side`, +1 right of the lane's travel). A row lies on the same side of every other line as its lane does (DESIGN-tracks §16, wip/REVIEW-architecture-tracks.md suggested test 1).

## Read first
- `CODINGSTYLE.md` (C++ part).
- `src/junction/planar/planar_components.cpp`: `SolveLazy1` (~2178), its sides round (the two places that compute `lp`, `lq` and `sides->Side(lp, lq)`, ~2247 and ~2269); the prio row strands (~1890-1905: name `"prio:" + row.strand`, start offset `row.side * Right(h) * 2`).
- `src/junction/planar/planar_model.h`: `PrioRow`.

## Change (behind `PLANAR_ROWSIDE=1`; without it the code is exactly today's)
1. A file-local function in planar_components.cpp, just before `SolveLazy1`:
   `static int StrandSide(const Model &M, const PlanarLineSides &sides, const std::string &p, const std::string &q)` returning the side of strand p relative to strand q's travel (+1 right, -1 left, 0 unknown):
   - When `PLANAR_ROWSIDE` is not set: `lp = sides.LineOfStrand(p)`, `lq = sides.LineOfStrand(q)`; return 0 if either is `UINT32_MAX` or `lp == lq`, else `sides.Side(lp, lq)` (exactly today's test).
   - When it is set, first resolve rows: a strand named `prio:<L>` is a row of lane strand `<L>`; find the `PrioRow` in `M.prio_rows` whose `strand == <L>` (none found: return 0).
     - p is the row of L and q's line is L's line (`LineOfStrand(q) == LineOfStrand(L)`): return `row.side`.
     - q is the row of L and p's line is L's line: return `-row.side`.
     - otherwise replace each row by its lane strand L and apply the rule of the first bullet to the resulting names.
2. In both places of the sides round, replace the `lp`/`lq` computation and the `lp == UINT32_MAX || lq == UINT32_MAX || lp == lq || sides->Side(lp, lq) == 0` test by `const int side = StrandSide(M, *sides, clash.first.first, clash.second.first); if (side == 0) continue;`, and use `side` where `sides->Side(lp, lq)` was used.
3. Nothing else (the row strand stays as it is: straight, 2P, tied to the merge).

## Measure (baselines: main line `<S>/lo1-pv-<hub>-after.txt`; ST `<S>/st-pv-<hub>.txt`)
1. `PLANAR_STRIP=1 PLANAR_ROWSIDE=1`: `tools/junction/pv run build-rel T2 --out <S>/st2-pv-T2.txt`, then T3, then LL2 with `--tags` the 12 tags of `<S>/lo1-pv-LL2-after.txt` (as ST did).
2. `PLANAR_ROWSIDE=1` alone: T2 and T3 into `<S>/rs-pv-<hub>.txt`.
3. Per run, against the main-line baseline (and for 1 also against ST): variants built; best variant with area, WxH, track, wasted, ms; max ms; variants over 15000 ms (count); failure reasons grouped with counts.
4. For the T2 and T3 bests of 1: `PLANAR_STRIP=1 PLANAR_ROWSIDE=1 PLANAR_ONLY=<tag> JUNCTION_COMPONENTS_TOPOLOGY=<hub> JUNCTION_COMPONENTS_OUT=<S>/st2-<hub>.json timeout -s KILL 60 build-rel/openttd_test "planar: dump a mapped component layout" -s > <S>/st2-e2e-<hub>.txt 2>&1`: plan result, `MAPPED`, `VIOLATION` lines grouped by rule with counts, or the failure text.
5. If T2 in 1 still builds fewer than half its baseline variants: rerun T2 o1t3f1s0 alone with `PLANAR_STRIP=1 PLANAR_ROWSIDE=1 PLANAR_IIS=1 PLANAR_TIMES=1` through the test `"planar components: T2 from its topology text (mapped) at the probe's area"` into `<S>/st2-dbg-T2.txt`, and report the last round lines, the failure text and the `IIS (extra) sep` lines.

## Do not
- Touch the slot layer, paths, shape, ladders, the signal pass, the checker, or the composer's construction. Touch ports 4077/4079 or start a game client. Use git stash. Run two heavy jobs at once.

## Commit and report
- One commit on probe-st: `Probe: Junction prio rows take their lane's sides in the lazy loop (PLANAR_ROWSIDE)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Report: references read; commit hash; functions added; the tables of 3; the results of 4 and 5; any stop; `git status --porcelain`; `git log -1`. Finish with `git switch wt-tracks` and the rebuild. When you have reported, stop.
