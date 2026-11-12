# RECON: the prio element under a row stretch (C5a; RECON-stretch C5, D217)

Worktree nodes2, branch wt2-c5a at 841b1fe4ac, Release build-rel. Measured unless marked INFERRED. The proposal is a patch (RECON-prio-stretch.proposal.patch, untracked, src/junction_topology.cpp only, ~45 lines).

## 1. Which runs show it now
`[topologystretch]` "pushing any point later keeps the seed legal", Y part, with a scratch print of every non-empty violation list. Prio shows in exactly 3 runs, all LL_RR test spec: item 3, 4 and 5, s 1 (rows 0 1 2 3 4 5 8.. become 0 1 2 4 5 6 9..; box 62x25 becomes 58x26). Each reports `[rule 9 at 55,6: the joiner's signal before an ML merge must be Entry] [rule 9 at 55,5: missing prio]`. Nothing else is violated. T (both specs) and LL_RR tl 6 v 196: no Prio. Y runs: LL_RR 22, solve failures 19 (items 0-2 "y floor", items 6-11 "lane split run/bridge heads hug", items 3-5 s 2 "occupancy"), 3 solved, all 3 Prio-violating. (Items 3, 4 and 5 give the same layout: one row gap.)

## 2. Experiment 1: what happens
Seed (item_rows 0 1 2 3 4 5): the ML path 1 runs on y=4, the joiner (path 9) arrives from y=5 and merges at M=(55,4), the prio row is y=3 (tiles x 41..55, joins h1 on y=4). Path 0 comes from y=1 and its out fan S-bends (57,1)..(59,4), 4 columns east of M and clear of the row.
Stretched (item 3 s 1): ML on y=5, joiner from y=6, M=(55,5), the row would be y=4, steps 0..14 = x 55..41. Path 0's out-fan S-bend lands on its port row y=4, which the stretch did not move (port rows are not item rows): its tiles (53,2) (54,2) (54,3) (55,3) (55,4) (56,4) put a Left piece on (55,4) = row step 0. The stretch moves the ML row down but leaves the other path's port row, so the row side of M now IS that port row.
Which check fails and why: not CheckPrio itself. SignalJunctionTrackRoutes phase 0 (src/junction_topology.cpp, "3. The requirements", `fits`) requires every row tile s = 0..n_prio free (`tracks_at(tracks, row(s)) == TrackBits{}`); instrumented, the only failing clause is `ROW s 0 (55,4) occupied 16` (Track::Left), for routes 1 and 2 (the two copies of path 1), n_prio 14. `if (!fits) continue;` skips the element silently: no joins, no row, no row signals, and phase 4 (the joiner's Entry retype) follows `entries`, so the joiner keeps Block at (55,6). Then CheckPrio (src/junction_rules.cpp:1239 "must be Entry"; :1256 "missing prio") reports the two violations. Everything is derived from one cause: the prio footprint is not reserved.

## 3. Experiment 2: which relation is missing
`[topology]` reasons for the X chain: "prio element" (:2123, :2143) only floors njm after nex / lane merge (even_prio + 1, +2), i.e. the ML's own coverage. The occupancy family (add_occupancy, D217) knows tracks of edges and node tiles; the prio element's row (and joins) are drawn by the signal pass, so they are in no occupancy row. In the seed the row is free by accident of the box (the flat solve keeps other tracks 4 columns away); with a row gap the row side tile coincides with another path's port row. Site relation vs family: it is a family hole (the footprint of every prio element), not a site relation (D217).

## 4. Proposal (checked)
The prio element's footprint as an occupant family in add_occupancy, derived from the Prio rule (CheckPrio items 4 and 5: the row tiles hold only the axis and h2, the component touches nothing used, the tile beyond the open end carries no track on the facing edge) and from the pass's `fits`:
- For every row r whose lane path is a main-main movement and whose joiner-merge node njm has the ML axis X plus one non-axis bit (the joiner's half, in node_bits): row side y = ML row + 1 if the half touches NW (Upper/Left), - 1 if it touches SE (Lower/Right).
- Two occupants on that row: step 0 tile at njm with bits h2 = Corner(rev side, rev exit), and the run steps 1..even_prio (upstream) with bits X (the axis bits make the family's existing "touch" rule leave one tile free at the open end; the h2 tile has no east touch, so downstream gap is the plain 1).
- Both belong to node njm (edge UINT32_MAX) so the joiner's and the ML's edges at njm are related and unconstrained; the two parts of one footprint are related to each other.
- Footprints never group (a shared tile is never legal: the Prio rule wants step 0 to hold only h2) and are never `tie`s; they use the "apart" relation. On equal flat columns the footprint sorts first in its travel direction (the unrelated track stands downstream, as in the seeds).
Patch: Occupant gets `int footprint` (travel sign), the loop above, `related` clause, sort key, group guard, tie guard; one forward declaration of JunctionCornerTrack.

CHECKED (Release):
- `[topologystretch]` "pushing any point later": all 3 runs clean, no Prio anywhere (per-rule list empty), X runs unchanged (T 104/12/92, LL_RR 232/16/216), Y run counts and solve failures unchanged (T 10/4, LL_RR 22/19 now clean 3, tl 6 v 196 LL_RR 22/16 clean 6).
- The occupancy placement test (seeds: SamePlacement, columns, 41x14, 62x25) and the whole wide gate `jt gate build-rel`: All tests passed (448 cases, 56 s on this machine against 120 s budget); this covers `[topologysweep]` (44 cells clean, row chains), `[topology]`, `[nodeoracle]`.
- Placement speed and reach: `[placeoracle]` before = after: solved=132, violations=0, 95.5-96.3 ms for 243 placements; `[realterrainsweep]` before = after (3 / 18 hard / 17 terraform solved, same failures).
The X-phase solve now sees the footprint, so the 3 runs change in X too (e.g. the out fan gets pushed east of the row); the Y runs are not made impossible: items 3-5 s 2 remain "occupancy" failures as before (not measured whether the footprint is the cause there).

## 5. Not measured (INFERRED)
- Whether other topologies (only T and LL_RR exist in the tests) need the h2 bits convention for west heading rows: the code takes both headings; the T's west prio is exercised in the 10 T Y runs and 104 X runs, all clean.
- The Y-floor rows s 2 failures (items 3-5) as footprint-caused: the failure reason still prints "occupancy" with a different edge pair; not examined.
- A joiner entering from the side of a bend ML (node with no X bit) is skipped like the pass skips it ("ML merge is not on an axis piece").
