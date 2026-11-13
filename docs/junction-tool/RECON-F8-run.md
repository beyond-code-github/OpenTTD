# F8 recon, run (read-only, worktree .claude/ng1 at 87b5e522e8, release build, throwaway tests reverted)

jp = src/junction/placement/junction_placement.cpp (line numbers at 87b5e522e8). Hubs run through a temporary test: placement of
PlaceJunctionTrackGraph with the seed's choices (where a seed exists) and with [.g3loop]'s all-floor choices (all-floor choices
taken from a recorded PlaceTrackGraph system, as the g3loop test does for an unseeded hub). Graph/programs/cut = the seed's when seeded,
else MakeJointInputs programs + DefaultCut (as g3loop). The earlier recon (RECON-F8-doubled.md) read stale programs: see Q1.

## 1. Doubled row edges, roles, tokens, outcomes
Notation: Over(x)=bridge over edge x, SB/M = SBend LaneMerge, SB/S = SBend LaneSplit (hand,item shown as (h,i)).
Seeds exist ONLY for L_R x L_R and LL_RR x L_R. All other hubs: no seed (L_R x L_RR "does not settle", L_R x LL_RR occupancy
edge 6 vs 12, LL_RR x LL_RR bridge-heads-hug conflict).

| hub | choices | outcome |
|---|---|---|
| L_R x L_R (T) | seed | placed |
| L_R x L_R | all-floor | FAIL: waiting room (edge 8, node 4) <- row nodes <- stack rows (row 1) <- joiner_div Corner choice (edge 5) <- chain run (edge 1) <- S port row <- S edge (node 4) ... (cycle) |
| LL_RR x L_R | seed | placed |
| LL_RR x L_R | all-floor | FAIL: chain run (edge 8) <- S-bend size (edge 8) <- chain run (edge 7) <- Corner size (edge 7) <- bridge heads hug crossers (edge 10, row 1) <- lane split run (edge 10, node 1, row 1) ... |
| L_R x LL_RR | all-floor | FAIL: chain run (edge 11) <- S-bend size (edge 9) <- chain run (edge 9) <- chain run (edge 10) <- bridge heads (edge 10) <- bridge over S-bend (edge 10) ... |
| L_R x L_RR | all-floor | FAIL: joiner_div Corner choice (edge 8) <- chain run (edge 2) <- S port row <- S edge (node 6) <- waiting room (edge 11, node 6) ... (no doubled edge at all) |
| LL_RR x LL_RR | all-floor | FAIL: S-bend size (edge 4) <- fan size (edge 4) <- S port spacing <- fan size (edge 7) <- ... bridge over S-bend (edge 5/8) ... (no e44 in the chain) |
(Full error texts: scratchpad f8recon2/q1.txt.)

Doubled edges (identical programs at seed and at all-floor; the programs do not depend on the choices):
- L_R x L_R: e7 leg: edges=[Over 6, Over 3, SB/M(-1,4)] copies=[SB/S(-1,4), Over 6, Over 3]; bridge b1[6 3].
- LL_RR x L_R: e10 leg (Over 7,13; SB/M(1,4); copy SB/S(1,4)); e15 leg (Over 3,7,8; SB/M(-1,7)); e16 leg (Over 17,3,7,8,11; SB/M(-1,10)).
- L_R x L_RR: e10 leg (Over 6,5,9; SB/M(-1,4)). L_R x LL_RR: e13 leg (Over 7,6,12,11; SB/M(-1,4)).
- LL_RR x LL_RR: legs e19 (Over 24,26; SB/M(1,4)), e28 (Over 15,14,17,16,8,5; SB/M(-1,7)), e29 (10 Overs; SB/M(-1,10)) and
  OUT e44: edges=[Over 14, Over 15, SB/M(-1,3), Fan(0,3)] copies=[SB/S(-1,3), Over 14, Over 15]; bridge b17[14 15] (cut) (g=1 or 2 per cut).
  The out edge carries BOTH roles (merge in edges, split in copies), the Fan only in edges (copy 0). Lane item 4, copy/north item 3.
- NO doubled IN edge in any of the five hubs. Scanned 144 hub pairs of {L_R,LL_R,L_RR,LL_RR,LLL_R,L_RRR,LLL_RRR,LL_RRR,LLL_RR,LR_LR,...}:
  in-edge doublings occur only with a 3-lane group (LL_R x LLL_RRR, LL_R x LLL_RR, LL_RR x LLL_RR/RRR, LLL_R x ..., LLL_RR x ...);
  every one of those, run through the same harness, fails before the row block: "a bridge over several band crossings is not ruled yet"
  (graph seed AND placement). So the in-edge statements below are STATIC (program + code), not run.
  Programs per junction_topology.cpp ~1131-1146: in edge edges=[Fan(a), Over..., SB/M(b)], copies=[SB/S(b), Over...] (copy 1 has no fan).

## 2. First mishandling in jp (instrumented, LL_RR x LL_RR, out edge e44; other hubs have no in/out doubling)
Instrument printed per doubled row edge. For e44: out node (edges[e44].from) item = 4 (lane), exit_item = nodes[in_edge[r].to].item = 3,
S-bend target c = 3, programs.nodes[to] none (port).
OUT edge, in file order:
 (a) FIRST: jp:~744 chain builder, `el.index = row_item != NO_ROW ? row_item : programs.nodes[graph.edges[in_edge[r]].to.index].item`
     (the HeadsPair at no preceding fan/S-bend). For copy 0 of the out edge (Over before the merge) computes index = 3 (the IN node's
     item). Should be 4 = programs.nodes[edge.from.index].item (the out edge starts at the joiner-merge node on the lane row, copy 0 stays
     there until its merge S-bend to c). Printed: "e44 out c0 heads tok0/tok1 index=3", from-node item 4. Copy 1 heads: index=3 (row_item=3
     after SB/S to c), correct.
 (b) jp:~1586/1594 doubled block (`if (is_row_edge(e) && doubled_edge(e))`): exit_item = in_edge's node item = 3, used for
     `eq(1, bend1.first, item_pt[exit_item])` and `eq(1, bend0.first, item_pt[exit_item])`. For the out edge bend1.first (copy 1 split)
     must be on 4 (the node's item = lane) and bend0.first (copy 0 merge, after heads on lane) on 4; bend1.last, bend0.last = item_pt[3]
     by their own tokens (bend1.index = bend0.index = 3: correct). As coded both ends of both S-bends are on row 3: no row change.
     Also nex = elems.front().first is the out NODE tile (printed "nex-is-NODE"): right for the split run. lane_bend finds both (found=1).
 (c) later sites that are leg-only: collect_bridges (jp:1428-1441) loops `leg_edge[r]` only, so in/out doubled heads give no BridgeLine/HeadRef;
     Fan token in result emission jp:~2519-2540 (`is_row_edge(e) && doubled_edge(e)` branch): a Fan token of edges[e] falls to the `else` Over
     branch -> bridge_over(e, fan item index) -> "a crossing edge misses its bridge" (static); sym_routes jp:~893-905 for c==1 takes
     programs.copies[e] which has no Fan token, so copy-1 routes omit the out/in edge's fan bend (static, no Fan in chain_copy).
 Experiment: patching (a) and (b) for the out edge (start item = programs.nodes[edge.from].item) changes heads index to 4 and the system
 still contradicts, but the conflict chain no longer contains e44 (it was never in it): LL_RR x LL_RR's failure is the legs e19/e5/e3
 (hug crossers, bridge over S-bend), independent of the out edge. Patch not kept.
IN edge (static): chain builder is right (copy 0 row_item from the Fan = a; copy 1 from its SB/S = b). First wrong site in file order
 would be row_changes jp:~795 kind 2 (lean toward chain[e][1].first = the Fan element, probably benign) then row_pieces jp:1071-1085 where
 `split = find_role(1, LaneSplit)` and `i == 0`: copy 0 (c==0) gets a = split->first for the run Tile->Fan (the run from the port tile starts
 at copy 1's split instead of the tile; wrong: a Fan follows, the run should be empty/shared), copy 1 gets copy -1 for the run port tile ->
 split (should be: shared = Fan + the run from the fan end to the split; fan is only in copy 0's chain). Confirmed wrong then in the block:
 exit_item = item of in.to node = b; bend1.first must be a (the fan item = programs.edges[e][0].index) and bend0.first (after the heads on
 a) must be a; with the coded exit_item=b both S-bends start on b. And nex = elems.front().first is the PORT tile, but the split run is
 measured from the fan end (FloorKind Fan -> LaneSplit), not Node -> LaneSplit.
 Also unchecked for in/out edges, jp:1741-1770 floors use `elems = chain[e]` (not reached for doubled: the block `continue`s first).

## 3. bridges_of[e] in travel order?
Yes for all 14 doubled edges listed (every hub run above, incl. e44 out): for each doubled e, the concatenation of structure.bridges[bridges_of[e][*]].crossed
equals the Over tokens of edges[e] in program order and equals copies[e]'s Over order (checked by script over q1.txt: 14 ok, 0 mismatch).
Cuts split by crossing count but keep order (e16: b4[17] b5[3 7] b6[8 11]; e29: b10[31 30] b11[15 14] b12[17 16] b13[8 5] b14[22 20]; e44: b17[14 15]).
Holds by construction: jp builds bridges_of by iterating structure.bridges (header: "sorted by edge; an edge's bridges in its travel order"),
chain builder picks the bridge by crossed[0]==Over index in program order. So bridges_of[e][b] is the b-th HeadsPair of both chains.
(DefaultCut cuts in series in order too.)

## 4. JunctionSignal switch events for in/out edges
junction_signals.cpp:47-80 (JunctionRouteEvents): per route, for every edge on its path with copies[e] non-empty: split_token = first SBend with
role LaneSplit in copies[e]; merge_token = first SBend with role LaneMerge in edges[e]. With the programs above BOTH are found for an in edge
(split = copies[0], merge = last token of edges) and for the out edge e44 (split = copies[0], merge = edges token 2, before the Fan): neither is
program.size(). The loop is role-independent (no leg test); it needs a sibling route (same path, other copy) and then emits the split event at the
tile of copy 1's tag (edge e, copy 1, token split_token) and the merge event at the tile of copy 0's last tag of (e, copy 0, merge_token).
So the earlier recon's "no event for in/out" is not true for the programs; the silent-miss risk is elsewhere: copy 1's route tags at an in/out
edge only carry tokens of copies[e] (no Fan), the placement's route emission for them must supply tag.token == split_token for copy 1 and
merge_token for copy 0, which depends on the unfixed placement sites in Q2 (not run: no in/out doubled hub places today).
Minor: on_tile(copy0.pieces[i].tile) searches `route`'s pieces, as for the leg.

## Notes
- Old recon items now stale: jp:670-676 refusal (`has_role` both) passes for in/out since both programs carry merge in edges and split in copies.
- Reverted: git status in .claude/ng1 clean (only branch wt-f8recon at 87b5e522e8); build-rel pch deleted/rebuilt only.
