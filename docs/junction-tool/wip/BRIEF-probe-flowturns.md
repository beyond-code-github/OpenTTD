# Probe: put a crossing's turn where the flow puts it (T2), solve, compare area

Throwaway probe: nothing committed. Time box: 60 minutes; at the box, report what you have. Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes; build only `openttd_test` in `build-rel`, never `openttd`. Outputs under `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/flowturns/`; code read with offset/limit; command output to files. If anything does not fit the code as described, stop and report; do not invent another rule.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, clean at `{HEAD}`. Read `/home/pete/OpenTTD/docs/junction-tool/wip/SPIKE-shapeflow2.md` (Part 3: on T2 o0t2f1s1 all 8 LP1 differences move one unit of turn along a line across a crossing, placed by the crossing path piece rule in `BuildPlanarPaths`). Apply `wip/spike-shapeflow2.patch` (it prints LP1 per stretch with `PLANAR_SHAPEFLOW=1`). Read `planar_paths.{h,cpp}` (`BuildPlanarPaths`, the crossing piece rule) and how the composer consumes the paths.

## Change (env `PLANAR_FLOWTURN=<list>` probe only)
- In the crossing piece rule, allow the turn unit of a named (line, crossing) to be placed on the other side of the crossing (after instead of before, or before instead of after), as LP1 says. The list names the (line, crossing) pairs; take them from LP1's differences on T2 o0t2f1s1 (print them first with the patch). If the rule has no such two-sided placement you can switch by a flag, stop and report how the turn is placed.
- Check with `PLANAR_SHAPEFLOW=1` that the built shape now matches LP1 on those stretches (rough sum |b| 8 instead of 12).

## Runs
- T2 o0t2f1s1: today vs all 8 moved; then each of the 8 moved alone (8 runs). Same for the other T2 variants whose LP1 differences are crossing moves (as many as time allows, list which).
- Per run: solved?, area, track, WxH, ms, sum |b| rough, first-round clashes, and any failure reason.

## Report (findings under 30 lines in `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-flowturns.md`)
Answer plainly: does less net turning give a smaller (or larger) layout here, and by how much? Then `git diff > /home/pete/OpenTTD/docs/junction-tool/wip/probe-flowturns.patch`, `git checkout -- .`, `git status --porcelain` empty, rebuild `openttd_test`. Report in under 12 lines. When you have reported, stop.
