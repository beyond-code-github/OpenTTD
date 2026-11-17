# Diagnosis: why rungs miss T2 f1's C/F clashes, and the slack -1 rows

Throwaway: nothing committed. Time box: 45 minutes. Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes; build only `openttd_test` in `build-rel`, never `openttd`. Outputs under `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/diagl/`; code read with offset/limit; command output to files. This is diagnosis: change no rule, report facts.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, clean at `{HEAD}`, Release `build-rel`. Apply `/home/pete/OpenTTD/docs/junction-tool/wip/probe-ladders3.patch` (faces from the flipped events, walls, rung rows). Read `wip/PROBE-ladders3.md`.

## Question 1: is the layout's embedding the face map's?
On T2 o0t2f1s1 (rough solve at HEAD, which is legal):
- At the clash site of C1/C2 vs F1#4/F2#4 (from the lazy loop's round-0 clash list), take a cross-section line through the clash point, perpendicular to F's segment there. List every strand segment it meets, in order along the line, with the strand name, segment index and heading.
- From the face map (flipped events), list the faces that contain C1, C2, F1#4, F2#4 and their walls, and say what lies between F and C according to the faces (which lines' stretches).
- Answer plainly: does the layout put C directly beside F where the faces say another line (which?) lies between them? If so, where is that line in the layout at that cross-section (does its stretch end, turn away, or sit elsewhere)?
- Also report the resolved site sides and tie choices of the S fray (CD) and S join for t0 and t2 (from `ResolvePlanarSites` or the composer), and the node leg order the topology authors at those nodes, so we can see if a tie changes the leg order relative to the face map.
- Repeat the cross-section listing on T2 o0t2f0s1 (f0, where rungs remove every clash) at the same pair if they are near each other there.

## Question 2: the slack -1 rung rows
On T2 o1t2f0s0 and T3 o1t2f0s0 (rows violated on the rough solve at slack -1, area +74 / +91): list each violated row: the point (strand, segment end), the facing segment (strand, index, heading), the direction used, the row's value on the rough solve, and the two pieces' actual minimum distance in the rough layout. Then read the clash rule (`JunctionTracksMayShareTile` and the lazy loop's `SepOptions`) and say what spacing the clash rule really requires between that point and that segment for those headings. Is 2 along `Right(h)` stricter than the rule for diagonal facing segments or diagonal-to-axis pairs?

## Report
Findings under 35 lines: `/home/pete/OpenTTD/docs/junction-tool/wip/DIAG-ladders.md`. Then `git checkout -- .`, `git status --porcelain` empty, rebuild `openttd_test`. Report in under 12 lines. When you have reported, stop.
