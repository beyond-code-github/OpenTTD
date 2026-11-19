# Brief ST4 (probe): prio coverage as a sum over the lane's pieces; the row follows the lane (on probe-st)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, on `probe-st` at 6d7adae860 (`PLANAR_STRIP`, `PLANAR_ROWSIDE`, coverage kept strict). Commit only on probe-st. Build dir `build-rel`, target `openttd_test` only. Finish with `git switch wt-tracks` and rebuild `openttd_test` in build-rel.
- Time box: 120 minutes. One heavy job at a time. pv `timeout -s KILL 900`; single runs `timeout -s KILL 60`.
- S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`.

## Why
Rulings: D108 (coverage is the main line's length from its last switch), D342 (coverage may bend; the row need not be straight; reach judges), D349 (the row is a track beside its lane, away from the joiners), DESIGN-tracks §13.1 (the row is an offset copy of the lane's coverage pieces). Today the prio join builds coverage as ONE straight segment `AddSeg(S, h, 2P + 2)` and the row as a fixed straight 2P strand beside the merge (`M.prio_rows`, built in the box loop ~1897). Evidence that this shape holds layouts open: when the ST probe wrongly let that segment shrink, T3 fell from 1907 to 1687.

## Read first
- `CODINGSTYLE.md` (C++ part).
- `src/junction/planar/planar_components.cpp` on probe-st: `MappedComposer::Join` (~1059-1120: the two coverage segments inside `StrictScope`, the near lanes' `Double`s, `M.prio_rows.push_back` ~1109); the box loop's prio-row block (~1896-1912); `StrandSide` (~2190); `M.copies` and `CopyRec` (`planar_model.h`).
- Reference shape (read only, do not cherry-pick): `git show archive/probe-pr:src/junction/planar/planar_components.cpp`, lines 1048-1058 (`first_seg`: coverage from the lane's last copy rejoin) and 1108-1124 (row strand tied segment by segment).

## Change (behind `PLANAR_COVBEND=1`; without it the code is exactly today's)
In `MappedComposer::Join`, prio case only:
1. Before the lanes' join segments are added, per lane k record `from[k]`: the largest `resume` over `M.copies` entries with `strand == S[k]`, else 0. (Copies added later inside this Join do not count for this value.)
2. The two coverage segments become `AddSeg(S[0], h, 2)` and `AddSeg(S[j], h, 2)` (no `StrictScope` needed).
3. After every lane's segments are added (just before `ms` is filled), per lane k: `const TagScope ts("prio coverage"); M.Add(Ge(sum of S[k]'s segment L from index cov[k] to its last segment, 2 * P + 2));` where `cov[k]` is `from[k]` for lane 0, and for a near lane j the `resume` of the last copy that this Join added for S[j] (its last `Double`), i.e. the largest `resume` over `M.copies` with `strand == S[j]` at that point.
4. Rows: keep `M.prio_rows.push_back(...)` as today (the side lookup uses it). Then per lane k build the row strand `"prio:" + S[k]` here: `NewStrand(name, M.NewPoint())`; tie its start to the start point `a` of lane segment `cov[k]` offset by `-sigma * 2 * Right(d)` with d that segment's heading; then for each lane segment g from `cov[k]` to the last: `rs = M.AddSeg(name, g.d, 0, std::nullopt, 0)` and, under `TagScope("prio row offset")`, `M.Add(Eq(Dot(Right(g.d), rs.b) - Dot(Right(g.d), g.b), -sigma * 2))`. Under `TagScope("prio row")` add the box rows for every row segment's end point exactly as the box loop does for its row (`Ge(p[0] - M.X0, 0)`, `Ge(M.X - p[0], 0)`, `Ge(p[1] - M.Y0, 1)`, `Ge(M.Y - p[1], 0)`) and for the start point.
5. In the box loop's prio-row block: skip a row when a strand named `"prio:" + row.strand` already exists.
6. Nothing else. A construction failure (`M.error`) or a parity failure: record the text, continue with the measurements.

## Measure (baselines: main line `<S>/lo1-pv-<hub>-after.txt`; strip with coverage kept `<S>/st3-pv-<hub>.txt`; LL2 always with `--tags <S>/ll2-12.txt`)
1. `PLANAR_COVBEND=1` alone: pv T2, T3, LL2 into `<S>/cb-pv-<hub>.txt`.
2. `PLANAR_COVBEND=1 PLANAR_STRIP=1 PLANAR_ROWSIDE=1`: pv T2, T3, LL2 into `<S>/cbs-pv-<hub>.txt`.
3. Per run against both baselines: variants built; best variant with area, WxH, track, wasted, ms; max ms; count over 15000 ms; failure reasons grouped with counts.
4. For the T2 and T3 bests of 1 and of 2: `<env> PLANAR_ONLY=<tag> JUNCTION_COMPONENTS_TOPOLOGY=<hub> JUNCTION_COMPONENTS_OUT=<S>/cb-<run>-<hub>.json timeout -s KILL 60 build-rel/openttd_test "planar: dump a mapped component layout" -s > <S>/cb-e2e-<run>-<hub>.txt 2>&1`: plan result, `MAPPED`, `VIOLATION` lines grouped by rule with counts, or the failure text.
5. If any run builds fewer than half the baseline's variants: rerun that hub's first failing variant alone with `PLANAR_IIS=1 PLANAR_TIMES=1` and the same env through the hub's mapped test (`"planar components: T2 from its topology text (mapped) at the probe's area"` or `"planar components: T3 from its topology text (mapped) at the hand composition's area"`), and report the failure text and the IIS rows grouped by tag family.

## Do not
- Touch the slot layer, paths, shape, the lazy loop, ladders, `Fray`, `Double`, the signal pass or the checker. Change the plain (non-prio) join. Touch ports 4077/4079 or start a game client. Use git stash. Run two heavy jobs at once.

## Commit and report
- One commit on probe-st: `Probe: Junction prio coverage summed over the lane's pieces, the row follows it (PLANAR_COVBEND)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Report: references read; commit hash; what was added; the tables of 3; results of 4 and 5; any stop; `git status --porcelain`; `git log -1`. Finish with `git switch wt-tracks` and the rebuild. When you have reported, stop.
