# Probe: ladders on every face boundary run, rung spacing from the clash rule

Throwaway probe: nothing committed. Time box: 60 minutes; at the box, report what you have. Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes; build only `openttd_test` in `build-rel`, never `openttd`. Outputs under `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/ladders4/`; code read with offset/limit; command output to files. If anything does not fit the code as described, stop and report; do not invent another rule.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, clean at `{HEAD}`, Release `build-rel`. Apply `/home/pete/OpenTTD/docs/junction-tool/wip/probe-ladders3.patch`. Read `wip/PROBE-ladders3.md` and `wip/DIAG-ladders.md` (both short).
- Diagnosis: (1) with the flipped face map, T2's face 22 walks F B F B: two runs on one side list, antiparallel (F2#2, C2#1), and rungs only paired left points with right segments, so F2 vs C2 got no row; (2) the rung distance 2 along Right(h) is stricter than the clash rule when the target segment is axis and the point belongs to an oblique piece or a perpendicular axis end (the rule needs 1 there).

## Changes
1. **Every run against every other run:** split each face's boundary (line entries, box edges ignored) into maximal runs of one walk direction (F or B), in walk order. Each run is a chain of segments; its interior side is known: the face lies left of the walk, so for an F run the face is left of travel... use exactly the convention the probe already uses for L and R walls (B run = left wall, face right of its travel; F run = right wall, face left of its travel). For every ordered pair of distinct runs (X, Y) of a face: every end point p of X's segments (off-wall points dropped as now) gets a row against the facing segment of Y (same nearest-facing-segment rule on the rough solve), keeping p on Y's interior side by the spacing below. This replaces the L-vs-R pairing (for two-run faces it gives the same rows).
2. **Spacing from the clash rule:** the row's constant is 2 along `Right(h_target)`, except 1 when the target segment is axis and the point's own segment (the segment of X whose end p is; for a shared end, take both segments and use the smaller constant) is diagonal or perpendicular to the target. Write this as one small function with the DIAG findings in a comment; do not add other cases.
3. Everything else unchanged (flipped face map, no copies, rows from the rough solve, lazy loop as the safety net, up to 3 re-read passes).

## Runs
- Full T2 and T3 variant sets; LL² on the 201 tags in `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/ll2-tags.txt` (use `-j 8` style parallelism if your harness allows; otherwise the four LL² variants o49t0f1s0, o1t3f0s0, o0t0f0s0, o1t0f0s0).
- Per hub: variants solved rough vs ladder, round-0 clashes rough vs ladder (total and how many variants reach 0), area better/same/worse vs rough (list worse ones with the delta), median and max ladder ms, rows violated on the rough solve (must be 0; list any), and every surviving clash pair.

## Report (findings under 35 lines in `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-ladders4.md`)
Then `git diff > /home/pete/OpenTTD/docs/junction-tool/wip/probe-ladders4.patch`, `git checkout -- .`, `git status --porcelain` empty, rebuild `openttd_test`. Report in under 12 lines. When you have reported, stop.
