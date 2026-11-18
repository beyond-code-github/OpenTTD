# Brief S4c (probe, throwaway): the prio join's entry gaps are a slot fact; staying stepped is free

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`. Branch `probe-s4b` is at {HEAD}. Create the branch `probe-s4c` from it and commit only there. Finish with `git switch wt-planar`.
- Time box: 75 minutes.
- Background: `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-S2.md`, sections "Run 3 (S2c)" and "Run 5 (S4b)".
  - **S2c:** lanes stay on their copies' tracks. LL2 beat the baseline on all 12 variants (median -135), but T2 failed at the B core into the C/B prio join, which needs the old entry gaps.
  - **S4b:** stepping back is charged. T2 equals the baseline, but LL2 lost its gain, because stepping back ties with width and the LP stepped back.
  - **The aim:** step back only where a component needs it.

## Changes (planar_slots.cpp only)
1. **Prio join entry gaps.** Take each site s whose kind is `PlanarSiteKind::Join` with `sites[s].prio`. Its straight channel is c = `sites[s].straight`, with n lines `cc.lines`; `cc.lines` order is left to right.
   - Let e be `{0, 1, 3, 5, ...}`, so e[0] = 0 and e[j] = 2j - 1. This is the composer's `JoinEntry(n)`.
   - Set far_left = `sites[s].side > 0`.
   - For j = 1 .. n-1, in c's last interval, add the row `s(cc.lines[j]) - s(cc.lines[0]) = far_left ? e[j] : e[n-1] - e[n-1-j]`.
2. **Width.** Run with `PLANAR_S2W=0`, so the objective is the steps only. Use the existing knob; it is not a code change. The intent is that staying stepped costs nothing and stepping back costs one step per line, so the line steps back only when a component's gap requires it.
3. Nothing else changes.

## Measure
- Build `openttd_test` in build-rel on probe-s4c.
- Run pv on T2, then T3, then LL2, one at a time, never in parallel, each with `PLANAR_S2W=0 PLANAR_S2LOG=1`, and each `--out` to the scratchpad.
- Then run T2 and LL2 again with `PLANAR_S2W=1`.
- Baselines (`build-rel/pv-*.baseline`, wt-planar 20f98229e0):
  - T2: 28 of 32, best 862.25.
  - T3: 28 of 32, best 1933.25.
  - LL2: 12 of 12, best 2354.25.
- **Report per run:**
  - built and failed, with reasons and counts;
  - best area against the baseline best;
  - the number better, worse and equal, and the median change;
  - the median ms against the baseline.
- **T3 (with W=0):** if any variant that builds at baseline fails, run `PLANAR_ONLY=<tag> PLANAR_IIS=1 PLANAR_S2LOG=1 PLANAR_S2W=0 build-rel/openttd_test "<T3's mapped test name>" -s` on the first such tag. Find the test name with `grep -n "T3 from its topology" src/tests/junction/planar/*.cpp`. Report its IIS rows with the composer file:line that adds each row, and its S2DIFF lines.
- **T2 (with W=0):** do the same for the first tag that builds at baseline but fails now, if any.

## Do not
- Touch `planar_components.cpp`.
- Change anything else.
- Land anything on `wt-planar`.
- Run two heavy jobs at once.
- Touch ports 4077 and 4079, or start a game client.
- Use git stash.

## Commit and report
- Commit on probe-s4c: `Probe: Junction S4c prio join entry gaps in the slot layer (throwaway)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Add a "Run 6 (S4c)" section at the top of `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-S2.md`. Do not commit it.
- Reply with:
  - the tables;
  - the IISes;
  - the commit hash;
  - `git status --porcelain`.

When you have reported, stop.
