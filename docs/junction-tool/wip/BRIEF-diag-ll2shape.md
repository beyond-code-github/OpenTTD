# Diagnosis: why the shape program's shapes fail or grow on LL²

Diagnosis only: change no committed code. Time box: 60 minutes. Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes; build only `openttd_test`, never `openttd`. Code read with offset/limit; output to files under `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/diagshape/`.

## Setup (do not touch /home/pete/OpenTTD/.claude/ng1: another agent builds there)
`git -C /home/pete/OpenTTD/.claude/ng1 worktree add /tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/diagshape/wt 21cdedd3e6` (detached), configure a Release build dir inside it with the cache options of `/home/pete/OpenTTD/.claude/ng1/build-rel/CMakeCache.txt`, build `openttd_test`. Remove the worktree at the end (`git worktree remove --force`).

## Read
DECISIONS D340/D341, DESIGN-tracks §12 (incl. the judgement calls), `wip/SPIKE-shapeflow3.md`, `planar_shape.{h,cpp}`, `PLANAR_IIS` in planar_components (IIS core printing), `PlanarLineSides` (the topology sides the lazy loop applies).

## Questions (with `PLANAR_SHAPE=program`, `PLANAR_ONLY=<tag>` on the LL² mapped test)
1. o49t0f1s0 and two o2*f0 variants ("infeasible with topology sides"): which side rule(s) fail, between which lines, and how does the program's shape differ from the default near them (per channel heading lists, side by side)? Plainly: does the shape make a line approach a crossing or node from the side opposite to the one the face map demands?
2. Two o4*f0 variants ("base infeasible"): `PLANAR_IIS=1` core (first 30 lines); which rows (curve spacing, train length, component, port, box)? Plainly: is the shape asking for turns too close together for the curve rules, or something else?
3. Two o5t* variants (+625 area): where does the extra area come from (compare default vs program layouts: box W x H, which channels are longer)?
4. From 1-3, name the missing ingredient of the goal or of the program's rules (e.g. side rules from the face map, minimum length between turns, turns near crossings). Give the smallest addition to the shape program that would have avoided each failure, in terms of headings at crossings/ends only, and whether it is a rule (derived) or an opinion (cost).

## Report
Findings under 35 lines in `/home/pete/OpenTTD/docs/junction-tool/wip/DIAG-ll2shape.md`. Report in under 12 lines. When you have reported, stop.
