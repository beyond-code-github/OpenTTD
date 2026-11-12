# RW6b: the signal pass and the level solve read the prio element from the placement (RW6 changes 3-4; memory "graph source": steps read the graph/placement, never a rendered layout)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes2 (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/nodes2 checkout -B wt2-rw6b <HEAD>` (given at launch). Never use git stash. Other agents work in /home/pete/OpenTTD/.claude/nodes and .claude/ng1: never touch them. Use absolute paths.
- RW6 part 1 merged (816491b55b): the prio element is a `Prio` Elem on the leg's chain (index = Prio item; first/last points; floors Node->Prio 1, LaneMerge->Prio 2), occupancy and facts read it. Not done: SignalJunctionTrackRoutes still re-derives the element from the routes (~:3467-3567), and prio_beside (P6, the level solve's prio tiles) too. RW6's step 0 found they agree on every seed (15,888 elements) on columns, row and n_prio; the BITS differ: occupancy uses h2 at s = 0 and X at s = 1..even_prio; the signal pass draws h2 at s = 0, X at odd s, X plus h2 at even s > 0, and h1 on the lane row at even s.

## Read first (once each)
CODINGSTYLE.md (C++ part); RW6's commit (`git -C /home/pete/OpenTTD/.claude/nodes2 show 816491b55b --stat` and the Prio Elem parts); src/junction_topology.{h,cpp} JunctionTrackPlacement, SignalJunctionTrackRoutes and its prio drawing, prio_beside; src/junction_levels.cpp's prio_beside use; every caller of SignalJunctionTrackRoutes (grep) incl. src/junction_nodes.cpp ~:2005.

## Changes
1. `JunctionTrackPlacement` gains `std::vector<JunctionPrioSpan> prio; ///< Per prio element, its tiles and drawn bits (placement frame), from the Prio Elem.` with `struct JunctionPrioSpan { std::vector<std::pair<TileIndexDiffC, TrackBits>> tiles; TileIndexDiffC merge; };` (junction_topology.h; doc comments). Filled by the placement from the Prio Elem: the tiles on the Prio item's row and the lane-row h1 halves, with the bits the signal pass draws today (the signal pass's pattern is the source of truth for bits: it is what gets built).
2. Occupancy reads the same tiles and bits (replacing its own h2/X pattern), so occupancy, facts, levels and the signal pass share one source.
3. SignalJunctionTrackRoutes gains `const std::vector<JunctionPrioSpan> *prio = nullptr`: when given, it draws exactly those tiles and bits; when nullptr (callers with no placement, e.g. junction_nodes.cpp's template path), it derives as today (that path is the old pipeline; leave it). Every caller that has a placement passes `&placement.prio`.
4. prio_beside reads placement.prio.

## Tests
1. **Outer (identity of what is built):** `tools/junction/jt identity build-rel` and build-rel outputs of `"[topologysweep]"`, `"[topologystretch]"`, `"[placeoracle]"`, `"[realterrainsweep]"`, `"[topoprojsweep]"`, `"[junctiongame]"` equal to launch HEAD's (times stripped), LL_RR circuit byte-identical: the rendered/signalled layouts must not change. Occupancy now sees more bits (X + h2 at even s, h1 on the lane row): if that changes any placement (an occupancy failure or a moved point), STOP and report (tile, edges, bits): it is either a real collision the old bits hid or a missing graph relation.
2. A `[topology]` test "topology: the prio element has one source": on T and LL_RR seeds, placement.prio equals the prio tiles/bits in the signalled layout. Red: the member does not exist (compile) is not a red; record it as a characterisation.

## Stop conditions
- Any built layout changes; occupancy with the drawn bits changes a placement; anything not covered.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `'[topoproj]' 5`; `tools/junction/jt identity build-rel`; the sweeps; `tools/junction/jt gate build-rel` (count, time). TESTS.md rows.
- One commit: `Codechange: Junction signal pass and level solve read the prio element from the placement`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus the callers changed and the identity result. When you have reported, stop.
