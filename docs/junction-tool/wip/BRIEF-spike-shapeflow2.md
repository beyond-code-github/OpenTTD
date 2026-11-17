# Spike 2: shape flow: tied crossing corners, jogs by cause, flow shapes solved

Throwaway spike: nothing committed. Time box: 90 minutes; at the box, report what you have. Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes; build only `openttd_test` in `build-rel`, never `openttd`. Outputs under `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/shapeflow2/`; code read with offset/limit; command output to files. If anything does not fit the code as described, stop and report; do not invent another rule.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, clean at `{HEAD}`. Read `/home/pete/OpenTTD/docs/junction-tool/wip/SPIKE-shapeflow.md` and `BRIEF-spike-shapeflow.md`; apply `wip/spike-shapeflow.patch` (`PLANAR_SHAPEFLOW`) and build on it. Read `/home/pete/OpenTTD/docs/junction-tool/DESIGN-pieces.md` §1-§3 (heading sequences by construction: route turns, width changes, crossing angles; four heading sources) and the composer code that builds each stretch's heading sequence (`Join`, `Bundle`, crossing pieces in `planar_components.cpp`; `planar_paths`).

## Part 1: tied crossing corners (LP2 fixed)
At a crossing vertex the four corners (one per incident face) are x, 4-x, x, 4-x in 45° units (opposite corners equal, neighbours sum to 180°), x in [1, 3], one variable per crossing; corner turns follow from the corners. Re-run LP2; report integral?, objective vs LP1 and rough, crossings whose angle changes.

## Part 2: jogs by cause
For every stretch with net turn 0 and heading changes in the rough (a jog), name its cause from the code that built it (which construction rule or heading source of DESIGN-pieces put those pieces there: e.g. a width change between neighbouring lanes, a crossing approach, a merge/split exit step, a bundle lane offset, a copy (bridge) step, other). Report per hub a table cause -> count, and for "other" list the stretches. Question: are jogs all predicted from the topology (width changes and crossings), i.e. no jog is a free choice?

## Part 3: flow shapes solved (only if Part 2 leaves time; stop and report if the composer has no single place to set a stretch's net turn)
Pick T2 o0t2f1s1 and LL² o1t3f0s0 (LP1 below the rough). For each stretch whose LP1 net turn differs from the rough, find where its heading sequence is built and whether that sequence can be built with the flow's net turn by an existing rule or choice (name it). If every differing stretch maps to an existing choice, set those choices (probe env), solve, and report area and legality vs today. If not, list the stretches and why not.

## Runs
Parts 1-2: T2 o0t2f1s1, T3 o1t2f1s1, LL² o1t3f0s0, then full T2 set for counts.

## Report (findings under 40 lines in `/home/pete/OpenTTD/docs/junction-tool/wip/SPIKE-shapeflow2.md`)
Then `git diff > /home/pete/OpenTTD/docs/junction-tool/wip/spike-shapeflow2.patch`, `git checkout -- .`, `git status --porcelain` empty, rebuild `openttd_test`. Report in under 12 lines. When you have reported, stop.
