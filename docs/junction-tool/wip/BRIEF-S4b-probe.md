# Brief S4b (probe, throwaway): a step back after the parallelogram is charged from the copy's track; width first

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`. Branch `probe-s4a` is at {HEAD}. Create the branch `probe-s4b` from it and commit only there. Finish with `git switch wt-planar`.
- Time box: 60 minutes.
- Background: `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-S2.md`, section "Run 4 (S4a)".
  - The remaining T2 IIS is the B core: its `post` run offset is 3 slots, because the lanes carry on along their copies after the doubled crossings (the carry-on row). The prio join after it needs the old gap of 1 (that IIS holds the C2 cross row and the tie at L1063).
  - The slot layer gives the lines no place to step back: the carry-on row fixes the next interval's slot to the copy's slot.
  - The ruling D351 says the line may step back later by a charged S-bend. D352 says width comes first.

## Change (planar_slots.cpp only)
1. **Carry-on row.** At `planar_slots.cpp:129` on probe-s4a, replace the equality `s(L, iv+1) - s(copy, iv) = 0` with a charged step from the copy's slot. Add two new non-negative integer variables `up` and `dn`, each with objective coefficient 1, and the row `s(L, iv+1) - s(copy, iv) - up + dn = 0`.
2. The step from `s(L, iv)` to `s(L, iv+1)` stays uncharged, as it is today for this pair.
3. Nothing else changes.

## Measure
- Build `openttd_test` in build-rel on probe-s4b.
- Run pv on T2, then T3, then LL2, one at a time, never in parallel, each with `PLANAR_S2W=10 PLANAR_S2LOG=1`. Write each to the scratchpad (`--out`, as in Run 4).
- Then LL2 once more with `PLANAR_S2W=1`.
- Baselines (`build-rel/pv-*.baseline`, wt-planar 20f98229e0):
  - T2: 28 of 32, best 862.25.
  - T3: 28 of 32, best 1933.25.
  - LL2: 12 of 12, best 2354.25.
- **Report per run:**
  - built and failed, with reasons and counts;
  - best area against the baseline best;
  - the number better, worse and equal, and the median change;
  - the median ms against the baseline.
- **If T2 still has "base infeasible":**
  - Run `PLANAR_ONLY=<tag> PLANAR_IIS=1 PLANAR_S2LOG=1 PLANAR_S2W=10 build-rel/openttd_test "planar components: T2 from its topology text (mapped) at the probe's area" -s` on the first failing tag.
  - Report its IIS rows and S2DIFF lines.
  - Also report which composer function adds each row (file:line).
- **If T2 builds:** also run `pv` T2 per-variant against the baseline and give the per-variant table (tag, baseline area, probe area).

## Do not
- Touch `planar_components.cpp`.
- Change anything else.
- Land anything on `wt-planar`.
- Run two heavy jobs at once.
- Touch ports 4077 and 4079, or start a game client.
- Use git stash.

## Commit and report
- Commit on probe-s4b: `Probe: Junction S4b step back after a parallelogram is charged from the copy (throwaway)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Add a "Run 5 (S4b)" section at the top of `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-S2.md`. Do not commit it.
- Reply with:
  - the tables;
  - the IIS, if any;
  - the commit hash;
  - `git status --porcelain`.

When you have reported, stop.
