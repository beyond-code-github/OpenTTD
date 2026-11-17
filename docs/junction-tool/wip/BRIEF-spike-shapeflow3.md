# Spike 3: shape program with the crossing rules (absolute headings)

Throwaway spike: nothing committed. Time box: 75 minutes; at the box, report what you have. Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes; build only `openttd_test` in `build-rel`, never `openttd`. Outputs under `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/shapeflow3/`; code read with offset/limit; command output to files. If anything does not fit the code as described, stop and report; do not invent another rule.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, clean at `{HEAD}`. Read `/home/pete/OpenTTD/docs/junction-tool/wip/SPIKE-shapeflow.md`, `SPIKE-shapeflow2.md`, `PROBE-flowturns.md`. Apply `wip/spike-shapeflow2.patch` (LP1/LP2 with `PLANAR_SHAPEFLOW=1`) and build on it. Read in `planar_paths.cpp` the crossing rules (`suits`: which over/under headings may cross; over must be axis; no parallel crossing) and how crossing pieces get headings.
- Finding: the net-turn flow relaxes the crossing rules, so its minimum (T2 8 vs rough 12) is a shape the paths cannot build (D at 45° over F).

## Change (env `PLANAR_SHAPEFLOW=3`)
LP3 = LP1 (vertex turns fixed except at crossings, ring fixed) plus absolute headings at crossings: for each crossing and each of its two lines, the line's heading at the crossing piece = (heading at the line's start, fixed from the rough) + (sum of b over the stretches before it, plus the fixed within-line vertex turns before it) - 8 k, k integer; h in [0, 7] integer. Add exactly the rules `suits` applies (over heading axis: h even; the under/over pair allowed by `suits`, encoded with binaries per allowed pair if needed) and the tied-corner relation of spike 2 between the two lines' headings at that crossing. Solve as a MIP with HiGHS. Minimise sum |b| as before.
- If a rule of `suits` cannot be written over these headings (it needs something other than the two headings), stop and report the rule.

## Runs
T2 o0t2f1s1, T3 o1t2f1s1, LL² o1t3f0s0, then the full T2 set and as many LL² tags from `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/ll2-tags.txt` as time allows.
Per run: LP3 objective vs rough sum |b| (and LP1, LP2), MIP ms, crossings whose headings differ from the rough (line, crossing, rough h, LP3 h), stretches whose b differs.

## Report (findings under 30 lines in `/home/pete/OpenTTD/docs/junction-tool/wip/SPIKE-shapeflow3.md`)
Answer: under the real crossing rules, is today's turning already minimal (LP3 = rough) or is there a buildable shape with less turning, and where? Then `git diff > /home/pete/OpenTTD/docs/junction-tool/wip/spike-shapeflow3.patch`, `git checkout -- .`, `git status --porcelain` empty, rebuild `openttd_test`. Report in under 12 lines. When you have reported, stop.
