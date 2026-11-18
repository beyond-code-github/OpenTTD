# Brief S4a (probe, throwaway): the slot layer models today's prio fray and join parallelograms

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`. Branch `probe-s2c` is at {HEAD}. Create the branch `probe-s4a` from it and commit only there. Finish with `git switch wt-planar`.
- Time box: 90 minutes. This is a probe. Its product is the measurement.
- Background: `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-S2.md`, the sections "Run 3 (S2c)" and "Run 3 IIS". T2 and T3 fail with "base infeasible" in every variant. Every IIS holds channel 10's core flex row at a slot gap of 2, against the prio Fray or the prio Join, whose shapes are fixed.

## Why
Today's composer builds a parallelogram inside each prio site:
- **Prio fray (`Fray`, `planar_components.cpp` ~:1005).** The lanes go far to near, and `ins[0]` is the far lane. Every straight leg `S[j]` with j ≥ 1 goes through `Double(S[j], h, -sigma, ...)` and ends one slot toward the far lane, with no step back.
- **Prio join (`Join`, ~:1047).** Lane `S[j]` with j ≥ 1 doubles over the joiners and ends one slot toward the joiners. That gives the merge points a gap one slot larger than the lanes before the join, and the rows go there. The out lanes then step back by a fixed amount.

The slot layer treats a site as a point. It puts Row tracks into the straight channel's `post`, which forces a gap of 2 there. It does not move the fray's straight legs.

## Read first
- `src/junction/planar/planar_slots.cpp` on probe-s2c, the whole file. Lines are pushed in `cc.lines` order, so slot order, left to right, is `cc.lines` order.
- `planar_components.cpp` on probe-s2c: `Fray` and `Join`, and `build_fray`, to see how `ins` is ordered (`rs.side < 0` reverses it).

## Changes (planar_slots.cpp only)
1. **No Row tracks.** In `BuildIntervals`, do not push `PlanarSlotKind::Row` tracks. Keep the enum value.
2. **Prio fray straight legs.** Take each site s whose kind is `PlanarSiteKind::Fray` with `sites[s].prio`.
   - Let c be the fray's channel (`channels.sites[s].channel`).
   - The far in line is `cc.lines.front()` when `sites[s].side > 0`, and `cc.lines.back()` otherwise. This is the composer's `ins[0]`.
   - Take each of that site's link steps (the "straight line across a site" loop) whose `from` is an in line of c other than the far in line. Do not give that link free up/dn step variables. Instead fix `s(to, first interval) - s(from, last interval) = d`, where d = -1 when the far in line is `cc.lines.front()` and d = +1 otherwise. The link is uncharged.
   - Record it in `links` with that step, as today.
   - The far line's link stays charged, as today.
3. **Prio join:** no change beyond (1). The composer's join adds its own shift and fixed step back.
4. Nothing else changes.

## Measure
- Build `openttd_test` in build-rel on probe-s4a.
- Run `PLANAR_S2LOG=1 /home/pete/OpenTTD/tools/junction/pv run build-rel T2 --out <scratchpad>/s4a-T2.txt`. Use `pv`'s own option for the out file; if it has none, copy `build-rel/pv-T2.txt` away afterwards. Then do the same for T3 and LL2, one at a time and never in parallel.
- Compare against `build-rel/pv-*.baseline`, from wt-planar 20f98229e0: T2 28 of 32, best 862.25; T3 28 of 32, best 1933.25; LL2 12 of 12, best 2354.25.
- **Report per hub:**
  - variants built and variants failed, with each reason and its count;
  - best area against the baseline best;
  - the number better, worse and equal, and the median change;
  - the median solve time against the baseline.
- If T2 still has "base infeasible" variants: run `PLANAR_ONLY=<tag> PLANAR_IIS=1 PLANAR_S2LOG=1 build-rel/openttd_test "planar components: T2 from its topology text (mapped) at the probe's area" -s` on the first failing tag. Report its IIS rows and S2DIFF lines.
- Give the S2DIFF lines of T2 o1t3f1s0.
- Run `tools/junction/jt fast build-rel '[planar]'`. List its failures (the slot tests pin values, so failures are expected). Do not fix them.

## Do not
- Touch `planar_components.cpp`.
- Change any other slot rule.
- Land anything on `wt-planar`.
- Run two heavy jobs at once.
- Touch ports 4077 and 4079, or start a game client.
- Use git stash.

## Stop conditions
- A prio fray whose straight legs cannot be matched to its in lines through the existing link loop.

## Commit and report
- Commit on probe-s4a: `Probe: Junction S4a slot layer models today's prio fray and join (throwaway)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Add a "Run 4 (S4a)" section at the top of `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-S2.md`. Do not commit it.
- Reply with:
  - the per-hub table;
  - the IIS, if any;
  - the commit hash;
  - `git status --porcelain`.

When you have reported, stop.
