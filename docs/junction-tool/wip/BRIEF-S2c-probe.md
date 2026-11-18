# Brief S2c (probe, throwaway): one parallelogram over consecutive doubled crossings; width weight

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`. Branch `probe-s2b` is at {HEAD} (wt-planar 20f98229e0 plus the S2 probe commit). Create the branch `probe-s2c` from it and commit only there. Finish with `git switch wt-planar`.
- Time box: 2 hours. This is a probe. Its product is the measurement.
- The background is in `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-S2.md`, Run 2 section only.

## Why
- **T2 and T3 fail** with "slot change inside one piece" in channel 7: x0 [1 2], then mid0 [0 3].
  - The composer's core (`build_core`, which calls `Double`) builds ONE parallelogram per lane over all of its crossings: it splits once before the first deck and merges once after the last.
  - The slot layer instead merges the line onto its copy's track after every `x<k>`.
- **LL² loses about 770 area.** Hypothesis: a unit width weight ties with a unit step, so the lanes stay apart instead of stepping back.

## Read first
- `src/junction/planar/planar_slots.cpp`: `BuildIntervals`, and the copy rows and the objective in `BuildPlanarSlots`.
- `src/junction/planar/planar_components.cpp` on probe-s2b: `Double` (~:974) and `build_core` (~:1395-1510), so that you see the single parallelogram.

## Change A (slot layer, on probe-s2c)
1. **In `BuildIntervals`:** take a line L that has a `Copy` track in both `x<k>` and `x<k+1>` of the same channel. In `mid<k>` (the interval between them), add L's `Copy` track on the same side of L as it is in `x<k>`. The track order rules stay as they are.
2. **In `BuildPlanarSlots`, the copy rows.** For every interval `iv` that holds L's copy, with a following interval `next`:
   - **If `next` also holds L's copy:** add `s(L,next) - s(L,iv) = 0`. Remove the carry-on row for this pair. The step charge stays as it is; it will be 0.
   - **Otherwise:** keep the existing carry-on row `s(L,next) - s(copy,iv) = 0`, uncharged, as today.
3. Nothing else changes.
4. **Commit A:** `Probe: Junction S2c one parallelogram over consecutive doubled crossings (throwaway)`.
5. **Measure A** as described under Measure.

## Change B (slot layer, on top of A)
1. Read the env var `PLANAR_S2W` (an integer, default 1). Use it as the objective coefficient of every interval width variable. Step coefficients stay 1.
2. **Commit B:** `Probe: Junction S2c width weight knob (throwaway)`.
3. **Measure** with `PLANAR_S2W=3` and with `PLANAR_S2W=10`.

## Measure
- Baselines already exist for wt-planar 20f98229e0: `build-rel/pv-*.baseline` in ng1. Check that their tag lists match T2 (32), T3 (32) and LL2 (12). If they are missing, run `pv run` and `pv save` on `wt-planar` first. In that case, rebuild `openttd_test` in build-rel on wt-planar, and only then switch branches.
- Build `openttd_test` in build-rel on probe-s2c. Then run `/home/pete/OpenTTD/tools/junction/pv run build-rel T2`, then T3, then LL2, one at a time and never in parallel. Do this for each of: A (W=1), B W=3, B W=10.
- **Report per hub, for each of the three runs:**
  - variants built and variants failed, with each failure reason and its count;
  - best area against the baseline best;
  - the number of variants better, worse and equal, and the median change;
  - the median solve time against the baseline.
- If any variant still fails with "slot change inside one piece", give its channel, intervals and slots (`PLANAR_S2LOG`), for the first three distinct cases only.

## Do not
- Touch the composer (`planar_components.cpp`).
- Change any other slot rule.
- Land anything on `wt-planar`.
- Run two heavy jobs at once. The CPU is unstable.
- Touch ports 4077 and 4079, or start a game client.

## Stop conditions
- After A, T2 fails in more than half its variants with a reason other than "infeasible". Report the first three distinct cases, skip B, and stop.
- The side of a copy in `x<k>` is different from its side in `x<k+1>`. Report it and stop.

## Commit and report
- Each commit message ends with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Add a "Run 3 (S2c)" section at the top of `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-S2.md`. Do not commit it.
- Reply with:
  - the per-hub table for the three runs;
  - the commit hashes;
  - `git status --porcelain`.

When you have reported, stop.
