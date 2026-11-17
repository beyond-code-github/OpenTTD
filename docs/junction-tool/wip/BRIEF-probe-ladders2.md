# Probe: ladders everywhere (box-edge corridors, bridge copies as walls)

Throwaway probe: nothing committed. Time box: 60 minutes; at the box, report what you have. Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes; build only `openttd_test` in `build-rel`, never `openttd`. Outputs under `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/ladders2/`; code read with offset/limit; command output to files. If anything does not fit the code as described, stop and report; do not invent another rule.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, clean at `{HEAD}`, Release `build-rel`.
- Read `/home/pete/OpenTTD/docs/junction-tool/DESIGN-tracks.md` §6-§8 and `wip/PROBE-ladders.md`. Apply `wip/probe-ladders.patch` (it builds on the corridor probe's code and holds the rung rows, the off-wall point drop and the passes). Keep everything it does; this probe only widens which corridors and which pieces get rungs.
- Last result: LL² round-0 clashes went to 0 with rungs; T2/T3 kept their clash rounds, every surviving clash C against F (e.g. C2#1 vs F1#4), whose shared face has a box edge, and box-edge faces had no rungs.

## Changes to the probe
1. **Box-edge corridors:** faces with one or more `R` (box) edges are no longer skipped. Walls: the `F` entries form the right wall, the `B` entries the left wall, as before, ignoring the `R` edges (the box edge takes the place of the start or end vertex). Walk order across an `R`: rotate so each wall is one chain in travel order. A face with only one wall plus box stays skipped (the box rows hold it); count those. The outer face stays skipped.
2. **Bridge copies as wall pieces:** a doubled stretch's copy strand (`X"..`, `X'..` from `M.copies`, base strand X) belongs to the wall of every face where X is a wall and, in the rough solution, the copy lies on the face's side of X (for a left wall the face is right of X's travel: copy offset along `Right(h)` > 0; for a right wall, < 0). Its segments join that wall's piece list: they are facing candidates for the other wall's points, and their end points are rung points against the other wall, exactly as base segments are (same nearest-facing-segment rule, same off-wall drop: a copy end point counts as on the wall).
3. Everything else unchanged: rows from the rough solve, lazy loop on as a safety net, re-read passes when a rung point leaves its facing span (at most 3 passes).

## Runs
- Variants: T2 o0t2f1s1, T3 o0t1f1s1, LL² o49t0f1s0, LL² o1t3f0s0; then every T2 variant (the full T2 set) if time allows.
- Per run: rough vs ladder area, ms, lazy rounds, round-0 clashes and the clashing pairs (lines, base or copy), rung rows, rows from box-edge faces, rows from copies, faces skipped (one wall plus box).
- Any infeasible run: `PLANAR_IIS=1` core (first 30 lines). Any rung row violated on the rough solution itself: report it (a construction bug).

## Report (findings under 35 lines in `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-ladders2.md`)
Then `git diff > /home/pete/OpenTTD/docs/junction-tool/wip/probe-ladders2.patch`, `git checkout -- .`, `git status --porcelain` empty, rebuild `openttd_test`. Report in under 12 lines. When you have reported, stop.
