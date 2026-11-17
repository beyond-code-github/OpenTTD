# Check: does the chord drawing predict the order of events across each corridor?

Throwaway check: nothing committed. Time box: 30 minutes; at the box, report what you have. Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes; build only `openttd_test` in `build-rel`, never `openttd`. Outputs under `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/rungorder/`; code read with offset/limit; command output to files. If anything does not fit the code as described, stop and report.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, clean at `{HEAD}`, Release `build-rel`.
- Read `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-corridors.md` and reuse `wip/probe-corridors.patch` (apply it: it already builds faces, walls in composer segments, crossing-record matching, and solves the best variants). Read `DESIGN-tracks.md` §6.
- Idea: a corridor (face) has a left and right wall from a start vertex s to an end vertex t. To write non-overlap as rungs, we need the order along the corridor of the events on both walls. Question: does the topology's chord drawing (`planar_topology.cpp` ~344-360: ports on a circle, each line's first path a straight chord, crossings at chord intersections) give the same order as good solved layouts?

## What to compute, per hub (T2 o0t2f1s1, T3 o0t1f1s1, LL² o49t0f1s0, o1t3f0s0), per corridor (faces without box edges, not the crossing triangle)
1. Wall vertices: the crossings where a wall passes from one line to the next, plus every crossing lying on a wall stretch's end. Nodes other than s and t: count them and leave them out (the chord drawing has no node points).
2. Chord positions: each crossing's point = intersection of the two lines' first-path chords (same construction as the topology's event order; copy the angle formula, do not change topology code).
3. Layout positions: each crossing's solved point (`CrossRec`).
4. In each drawing separately: axis u = from the first to the last wall vertex of the corridor in travel order (both walls together; if fewer than one vertex per wall, the corridor has no cross-wall order to check: count it). Project every wall vertex on u; the order is the left/right interleaving (sequence of L and R by projection).
5. Compare the interleaving sequences: agree / disagree.
6. Also count the bends (segment boundaries with a heading change) on each wall between consecutive vertices in the solved layout: these are rungs whose order the chord drawing cannot give at all.

## Report (findings under 30 lines in `/home/pete/OpenTTD/docs/junction-tool/wip/CHECK-rungorder.md`)
Per hub: corridors total, corridors with a cross-wall order to check, agree, disagree (list each: face, walls' lines, both sequences), interior nodes left out, bends per corridor (total and max). Then restore: `git checkout -- .`, `git status --porcelain` empty, rebuild `openttd_test`. Report in under 12 lines. When you have reported, stop.
