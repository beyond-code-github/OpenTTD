# Brief S4d (probe, throwaway): S-bends go as late as possible

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`. Branch `probe-s4c` is at {HEAD}. Create the branch `probe-s4d` from it and commit only there. When you finish, run `git switch wt-planar` and rebuild `openttd_test` in build-rel there.
- Time box: 60 minutes.

## Why
- On LL2 o0t1f0s1, S2c (2230.25) beats S4c (2354.25). In S2c, lane 1 stays on its copy's track after the doubled bridge on channel 7, and through channel 9's core `post`, at offset 3. In S4c it steps back straight after the bridge, to offset 1.
- In S4c's objective the two cost the same number of steps. The lane has to step back somewhere, either after the bridge or later, across a site link. The LP picked the early step.
- Area says late is better: the S-bend goes where the lanes have to be, not straight after the bridge.

## Change (planar_slots.cpp only)
1. **Late steps.** Give every step variable pair a cost that falls the later its step comes in its channel, so that, at equal step count, a later step wins.
   - For the step of a line from interval `iv` to `iv+1` in channel c, use cost `1 + 0.001 * (intervals[c].size() - 1 - iv)` on each of `up` and `dn`.
   - Use the same formula for the charged carry-on step from the copy (the `up`/`dn` added in S4b), with that pair's `iv`.
   - Site link steps keep cost 1.
2. Nothing else changes. Run with `PLANAR_S2W=0`.

## Measure
- Build `openttd_test` in build-rel on probe-s4d.
- Run pv on T2, then T3, then LL2, each with `PLANAR_S2W=0 PLANAR_S2LOG=1` and `--out` to the scratchpad `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`. Run them one at a time, never in parallel.
- Baselines (`build-rel/pv-*.baseline`): T2 28/32, best 862.25; T3 28/32, best 1933.25; LL2 12/12, best 2354.25. For reference, S2c's LL2 best was 2230.25, with a median change of -135.
- **Report per hub:**
  - built and failed, with reasons and counts;
  - best area against the baseline best;
  - the number better, worse and equal, and the median change;
  - the median ms against the baseline.
- If a variant that builds at baseline fails, run `PLANAR_ONLY=<tag> PLANAR_IIS=1 PLANAR_S2LOG=1 PLANAR_S2W=0` with that hub's mapped test name on the first such tag. Report the IIS rows, the composer file:line of each, and the S2DIFF lines.
- Give LL2 o0t1f0s1's S2DIFF lines.

## Do not
- Touch `planar_components.cpp`.
- Change anything else.
- Land anything on `wt-planar`.
- Run two heavy jobs at once.
- Touch ports 4077 and 4079, or start a game client.
- Use git stash.

## Commit and report
- Commit on probe-s4d: `Probe: Junction S4d slot steps go as late as possible (throwaway)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Add a "Run 7 (S4d)" section at the top of `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-S2.md`. Do not commit it.
- Reply with:
  - the tables;
  - any IIS;
  - the commit hash;
  - `git status --porcelain`.

When you have reported, stop.
