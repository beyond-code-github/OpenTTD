# Probe: free bends in the ladder solve, clash loop only in the rough

Throwaway probe: nothing committed. Time box: 75 minutes; at the box, report what you have. Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes; build only `openttd_test` in `build-rel`, never `openttd`. Outputs under `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/ladderbends/`; code read with offset/limit; command output to files. If anything does not fit the code as described, stop and report; do not invent another rule.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, clean at `{HEAD}` (ladders in; see `src/junction/planar/CLAUDE.md`). Read `/home/pete/OpenTTD/docs/junction-tool/DECISIONS.md` row D338, `wip/SPIKE-freebend.md`, `wip/PROBE-noclash.md`, and the two patches `wip/spike-freebend.patch` (free-bend slots, `PLANAR_FREEBEND`, on 05e0159b09, before ladders) and `wip/probe-noclash.patch` (detect-only clash rounds, `PLANAR_NOCLASH`, ladder re-read loop).
- Facts: the free-bend spike failed because the lazy clash loop fought the slots (bundle slots: T2 15/16 but at the 15 s budget; join slots: "a clash is not separable"; LL² nothing in 5 s). The no-clash probe failed on LL² because rungs read from an overlapped rough demand an impossible order. So: rough = today's solve (fixed shapes, clash loop on, legal); the ladder solve gets the free bends and no clash separation.

## Change (env `PLANAR_LADDERBEND=1`, probe only)
1. Port the spike's slots onto `{HEAD}` (bundle slots and join slots, as the spike built them, with its equal-diagonal Eq rows; big-M bound 16), but add them only to the ladder pass models, never to the rough model. If the port does not apply cleanly in a way you can follow exactly, stop and report.
2. Ladder passes run with detect-only clash rounds (as `PLANAR_NOCLASH` does in the noclash patch), under-deck handling as today; repeat (rungs from the latest solution) while a rung point moved off its span or the solution has clashes, at most 6 passes; if a ladder pass is infeasible, the previous legal solution stands (count it).
3. Rungs are read from the rough model (no slots) on pass 1, then from the latest ladder model. Note: segment keys differ once slots add pieces; map rung points and targets by strand and the slot-free segment index if the spike's slots keep base segment indices; if they renumber, stop and report how.
- Variant B (env `PLANAR_LADDERBEND=2`): bundle slots only (no join slots).

## Runs
- T2 o0t2f1s1, T3 o1t2f1s1, LL² o1t3f0s0 first; then full T2, T3, LL² on the 201 tags in `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/ll2-tags.txt` (`/home/pete/OpenTTD/tools/junction/pv run ... -j 6`, plus per-variant stderr logs). Both variants 1 and 2 if time allows (1 first).
- Per hub vs today: variants ending legal (0 clashes) vs solved today; ladder passes infeasible (rough kept); area better/same/worse with deltas (best area per hub); slots used (bends taken) in the best variant; passes histogram; median/max ms.

## Report (findings under 35 lines in `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-ladderbends.md`)
Then `git diff > /home/pete/OpenTTD/docs/junction-tool/wip/probe-ladderbends.patch`, `git checkout -- .`, `git status --porcelain` empty, rebuild `openttd_test`. Report in under 12 lines. When you have reported, stop.
