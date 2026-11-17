# Probe: ladders with the flipped-triangle face map, no copies as walls

Throwaway probe: nothing committed. Time box: 60 minutes; at the box, report what you have. Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes; build only `openttd_test` in `build-rel`, never `openttd`. Outputs under `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/ladders3/`; code read with offset/limit; command output to files. If anything does not fit the code as described, stop and report; do not invent another rule.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, clean at `{HEAD}`, Release `build-rel`.
- Read `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-ladders.md` and `PROBE-ladders2.md`. Start from `wip/probe-ladders2.patch` (apply it), then make the changes below.
- Evidence: on the full T2 set, rungs removed every round-0 clash on the f0 variants (triangle not flipped) but not on f1 (flipped); the surviving C-vs-F clashes have no two-wall face in common. Suspected cause: `PlanarLineSides` (and the probe's face map) is built from the topology's unflipped event order, while the composer uses `FlipPlanarTriangles` (planar_paths, per variant flips), so on flipped variants the faces near the triangle are wrong. Separately, copies as wall pieces made areas worse (+12 to +186) because a copy's deck is a bridge (deck middles may pass over ground tracks), which the rungs treated as ground.

## Changes
1. **Copies off:** remove copies as wall pieces (and the `PLANAR_COPYOFF` addition) so walls are base strands only, as in `probe-ladders.patch`. Keep the box-edge faces from probe 2.
2. **Flipped face map (probe only):** read `FlipPlanarTriangles` and `PlanarTriangles` (planar_paths) to see which channel crossings a flip swaps on each of the triangle's three channels. For the probe's face map, build it from a copy of the topology whose `events[line]` has the same swap applied on every line of those channels (the two triangle crossings' line-level events exchanged as blocks, in the order the flipped channel gives). Use that copy only for the probe's faces/walls; do not change `PlanarLineSides` or the composer's lazy-loop sides. Validate: the flipped face map still passes V - E + F = 2 and every face a simple walk; report face counts f0 vs f1.
3. Everything else as in probe 2: rows from the rough solve, lazy loop on as the safety net, re-read passes (at most 3).

## Runs
- T2 o0t2f1s1, T3 o0t1f1s1, LL² o49t0f1s0, LL² o1t3f0s0; then the full T2 set (32) and, if time allows, the full T3 set.
- Per run: rough vs ladder area, ms, lazy rounds, round-0 clashes and the clashing pairs, rung rows (from box-edge faces separately). Any rung row violated on the rough solve itself: list it. Any infeasible ladder run: `PLANAR_IIS=1` core (first 30 lines).

## Report (findings under 35 lines in `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-ladders3.md`)
Then `git diff > /home/pete/OpenTTD/docs/junction-tool/wip/probe-ladders3.patch`, `git checkout -- .`, `git status --porcelain` empty, rebuild `openttd_test`. Report in under 12 lines. When you have reported, stop.
