# RECON: a crossed lane in edge (LLL_RRR x LLL_RRR and wider)

Base 371103aa23. Probe: a temporary test dumping graph, order and crossings (reverted). Names below are real sides: main W-E, branch S.

## Facts
- Refusal: junction_topology.cpp:1067 (`!graph.edges[e].from.node` on a row edge with a doubled bridge). Hit by LLL_RRR x LLL_RRR, LLLL_RRRR x LLLL_RRRR and LLL_RRR x LLLL_RRRR (any pairing); T, LL_RR x L_R, LLL_RRR x L_R and LLL_RRR x LL_RR have no crossed in edge.
- LLL_RRR x LLL_RRR: the edge is e3, row 1 (W main lane 1), port W-in 1 to node 3 (the lane's first split, the exit split to the S branch), 4 paths (3 exits to S-out 0..2, 1 through to E-out 1).
- e3 is crossed by e54, e57, e60: joiners S-in 0, 1, 2 to E-out lane 0 (each one path, from the branch split nodes in the S band up to the merge tree of E-out lane 0 on row 0). They are joiners (branch to main), none are exits. Row 2's through leg e44 is crossed by their siblings (e54..e61) and already takes a doubled bridge (leg case, works today).
- LLLL_RRRR x LLLL_RRRR: e4 (W-in 1) crossed by the 4 joiners S-in k to E-out 0; e8 (W-in 2) crossed by 8 joiners (S-in k to E-out 0 and 1). LLL x LLLL: e4 only.
- Why it is on the in edge: the crossing set is fixed by the graph stage, not the structure. Path-chord crossings are assigned to the heaviest crossing edge pair first (junction_topology.cpp:470-492, weight = paths of i x paths of j; D201 by rule). e3 carries all 4 W1 paths, the leg only 1, each exit edge 3 or fewer, so the joiners cross e3. The order stage only realises it (check at :825-829: realised == graph.crossings), putting the crossing items before node 3 in row 1 (order rows: `e54 e57 e60 n3 n60 ...`).
- Why a bridge of the row edge: structure :912 makes every crossing set on a row edge a doubled bridge (JunctionTrackBridge::doubled, junction_topology.h:186: "main-line bridge (D80, D188)"; D80 every main-line bridge doubled, D188 per lane, D174 main-line bridges always doubled whatever they cross, D190 main line takes the grade separation first). D223/D227 over/under (:939-961) only covers two non-row edges.
- "Doubled" (D80, D100, D105, D118; DESIGN-G2 G2.4): two parallel bridges of equal pathfinder cost, copy 0 `programs.edges[e]` and copy 1 `programs.copies[e]`, each with one S-bend (LaneSplit/LaneMerge); copy 1's heads one tile upstream (D118, D145).
- Mirror at the out edge exists: e99, e102 (E-out 2, 1) carry doubled bridges over the W exit legs, built by G2.4/D229 (programs :1146-1155). The data is the time-reverse of the in edge: out edge crossed by exits, in edge crossed by joiners.
- Stack already has the Copy item for such a row (`crossed` at :875 counts any row edge, in edge included), so no stack change.

## Rules found (Q2)
- DESIGN-G2 G2.4 last line: "A doubled in edge is the mirror and is not built yet; it is refused by name." DESIGN-G1.md:119 (rows In, leg, Out; in edge Fan to the upstream item, north when the leg is doubled).
- D229: "Merge then bridge: the lane carries its doubled bridge on its in or out edge (copies split right after the merge; copies of one lane to one destination are not a D24 breach). Bridge then merge: the crossing stays on the lane's leg and the crossers also pass the joiner on its own single bridge." So the in edge is named legal. Caveat: for the in edge the copies re-merge before the exit split (node 3), so the D24 reading (merge before split) needs the engineer's word that D229's exemption covers it. No other in/out edge crossing rule found (grep of DECISIONS, G1, G2, RECON-wide-hubs, CARDS).

## Options
1. Mirror of G2.4 (doubled bridge on the in edge: port Fan to Copy item A, copy 1 S-bends A to Lane before the bridge, copy 0 stays on A, S-bends after; both end on the Lane where node 3 sits). Stages: structure, delete :1067 (one line, no stack change); programs, add an in-edge branch beside :1146 (Fan to Copy item, then Over/LaneMerge, copies `LaneSplit, Over`); placement, copy chains start at the port tile with a shared Fan head (chain builder :657-670 and R11, bigger step); signals read roles generically. T and LL_RR x L_R: unchanged, the new branch only runs where the refusal runs today.
2. Move the crossing onto the leg (crossers pass after the split, the D229 "bridge then merge" analogue). Crossers then also cross each exit edge (e4.. for 3 exits: 4 crossings per joiner instead of 1), each on its own single bridge. Stages: graph (:470-492 weight rule must prefer the leg, a new rule), order (items after node 3), structure, programs, placement (band edges over band edges, supported). Risk: weight rule is shared, so T and LL_RR x L_R crossings could move unless the rule applies only when the heaviest pair is an in edge.
3. Crossers go over (single bridges, D174, D190 "tried first" not forced). Stages: structure (:912 and a new row-edge over/under rule), programs (row edge needs an Under step; rows have none), placement (a Y-axis bridge over a row chain; placement :275 refuses). Largest change, contradicts the default that the main line takes the separation; today's layouts stay if gated to the in edge.

## Prior art checked
DESIGN-G2 G2.4; D229, D80, D188, D174, D190, D223, D227, D228; RECON-wide-hubs (tie-break, not this); DESIGN-G1 :119, :230; placement :275, :657-670, :1210; signals :47. No earlier note covers a crossed in edge.

## Recommendation (mine)
Option 1: it is the named mirror of built G2.4, reuses D229's ruling (ask only whether the merge-before-split of the copies at node 3 is acceptable under D24), and cannot change today's T or LL_RR x L_R.
