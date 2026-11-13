# RECON G2c: band-edge bridge over several crossings (at d1d35c0ece, read-only)

## Headline
The structure never makes "one band bridge over several": StructureJunctionTrackGraph (junction_topology.cpp:938-962) pushes
one single bridge per band crossing pair ({edge,{x},false}). A band edge with k crossings where it goes over has k
single bridges (series of 1) and k Over tokens. The refusal "a bridge over several band crossings is not ruled yet" is
really "bridges_of[e].size() != 1": it fires for k>=2 single bridges, not for one bridge with k crossed.
So today's gap is "series of singles", and "one bridge over k" needs a new merge step in front of it.

## 1. Sites assuming one bridge / one crosser per band edge (junction_placement.cpp unless noted)
- :275 THE REFUSAL, in the non-row chain builder: on an Over/Tunnel token requires bridges_of[e].size()==1, not doubled,
  crossed.size()==1; uses bridges_of[e][0]; emits one undoubled Elem K::Heads per Over token with el.cross = crossed[0].
  Needs (series of k): pick the bridge by token (find bridge whose crossed holds step.index), not [0]; one Heads per token is then right.
  Needs (one bridge over k): Heads only at the first crossed token, skip the others (as the row builder does :732-737);
  el.cross becomes a list.
- :1141-1142 leaving/joining pick chain[e][1] / [size-2] "k != Heads": unaffected unless a bridge starts at the edge's very start.
- :438-445 chain dump uses bridges_of[e][0].tunnel: debug only; wrong per-bridge for mixed bridge/tunnel in series.
- ~:1780-1800 "bridge over S-bend" (D113, F4): per Heads elem, finds ONE S-bend of chain[el.cross] and pins the heads to the
  column next to its start (lo/hi tile rows). One bridge over k needs the span to cover all k crossed S-bends (min lo, max hi),
  the series case works per Heads as is.
- :1412-1424 collect_bridges (clearance families, Y-axis heads): takes b = bridges_of[e][0] for every Heads elem
  of a non-row edge. Needs b of that elem's token (series). One bridge: one line/two heads, same as today.
  The doubled branch (:1426-1439) uses nth_heads(chain, k) per bridge in order; a band edge would use the same k-th-Heads idea.
- :2480-2495 heads_of (result): single bridge returns the FIRST K::Heads in chain[edge] whatever b is. Needs k-th Heads by b's index in bridges_of[edge].
- :2564-2570 result emission for an Over/Tunnel token: heads_of(bridges_of[e][0],0), tiles_under(step.index,...) strictly between those heads.
  Series: bridge of this token. One bridge: the same heads for all k tokens (tiles_under per crossed edge, already per token).
- :2575-2585 Under side: bridge_over(x,e) then heads_of(bx,0): fine once heads_of picks the right Heads.
- :2592-2606 bridge emission: one pair per structure bridge; single: copy 1 = {0,0}; no span-length check for singles (only doubled
  middle <= max_bridge_middle). One bridge over k has no length cap on single bridges here; D228 says game max_bridge_length: a check is missing.
- junction_render.cpp:49 bridge_over(e,x) finds the bridge per crossed edge; :94-110 skips repeated Over tokens of the same
  bridge (b == last_over) and emits ONE 'B<middle>' for the bridge from placement.bridges[b]. Already handles one bridge over k
  and series (each token maps to its bridge). Nothing to change.
- Programs (topology.cpp:1210-1219): per crossing in travel order: bridge_over(e,x) -> Over (Tunnel if tunnel) else
  Under/Above. One Over per crossed edge, no grouping. Fine for both. An Over token run with Unders between (see 2) is a natural series.
- Signals (signal_pass.cpp:63,91,144,...): work on layout pieces' on_bridge / FindBridgeByHead; no per-edge assumption. Bridge heads in series: no signal
  in the 2-tile floor between far head and next near head? Not checked.
- junction_levels.cpp:372,563,1497: iterate layout.bridges generically (heads share z, span ground within max_bridge_height). Longer span = more height cost only.
- CutJunctionTrackBridges (junction_pipeline.cpp:110): skips `!bridge.doubled`; only doubled bridges are cut, compositions capped by
  JUNCTION_BRIDGE_MAX_CROSSERS (topology.h:180, uses pipeline.cpp:64-119, CLAUDE.md:32,49; D228 says remove). Band edges get NO options.
- STRUCTURE ORDER BUG for band edges: bridges are stable_sorted by edge only, so an edge's bridges are in crossing-pair order
  (graph.crossings), not travel order, contrary to the struct comment. Seen: LLL_RRR x LLL_RRR edge 22: bridges (10, 15) but program
  Over:15 Over:10; edge 23: bridges (45, 48), program Over:48 Over:45. bridges_of[e][k]-by-index would pick the wrong one; look up by crossed edge instead.

Row-edge doubled bridge machinery vs a single band bridge:
- Crossed list: JunctionTrackBridge.crossed is already a list; row bridges set it from order.edge_crossings (:912); band ones never do. Reusable as is.
- Chain: row builder (:732-739) = one HeadsPair at the bridge's first crossed token, other tokens `continue`. Reusable idea; band needs the same on K::Heads.
- "bridge heads hug crossers" (:1626-1640): HugCandidate per bridge over every crossed edge's tiles on both copies' rows. Row/X-axis only, doubled only.
  A band bridge's equivalent is the "bridge over S-bend" column rule above + Y-axis clearance (add_clearance_x), not this.
- Series cut: Heads->Heads floor 2 (:81, "next near head 2 tiles past last far head", D209/11.9b) is in JunctionRunFloor keyed by Elem kinds, so a band chain
  Heads,Heads gets it if the chain run uses JunctionRunFloor (not verified by a run).
- Not applicable as is: HeadsPair, nth_heads, cut compositions, copies, lane split/merge floors, max_bridge_middle check.

## 2. MakeJunctionGraphSeed results (rig: throwaway test in ng1, reverted; output scratchpad/g2crecon/run4.txt)
- LL_RR x LL_RR keep: "a bridge over several band crossings is not ruled yet". Cause: edge 13 has two single bridges (over 6, over 7).
- LLL_RRR x LL_RR any: seed did NOT finish in 100 s (reaches placement; the cut search over doubled bridges with 4,8,8,12,16 crossers at cap 2
  is a huge compositions product). With the default cut (cap-2 chunks) one placement gives the same refusal: edges 7 and 12 each have two singles.
- LLL_RRR x LLL_RRR any: same refusal with the default cut; seed itself would be as slow (not run to the end).
- Extra: LL_RR x LL_RR any: seed fails differently: "bridge heads hug crossers (edge 19, row 1) <- lane split run ... bridge over S-bend (edge 5) ..." (contradiction, not G2c).
Band edges with >1 bridge (all singles with ONE crossed each; no single bridge in structure has crossed.size()>1):
- LL_RR keep: e13 [6,7]; Over:6 Under x4 (15,14) Over:7 -> NOT consecutive (two crossers between).
- LLL_RRR x LL_RR: e7 [61,66], e12 [61,66]: Over:61 Over:66 consecutive, no token between.
- LLL_RRR x LLL_RRR (8 band edges with 2 singles): consecutive Over tokens: e22 [15,10], e23 [48,45], e55 [57,60], e56 [16,21],
  e68 [69,70] (after Under pairs), e96 [72,73], e97 [85,90]; interleaved with Under pairs: e33 [5,8] (Over:5 Under:99 Over:8).
  All k = 2; no band edge has 3+ crossings.
- Spacing of crossers (tiles between) is a placement output; I could not print it (no placement succeeds). "Consecutive tokens" is the only
  evidence: consecutive Over tokens have no S-bend/corner between, so the crossers are neighbours whose gap is set by the crossers' own chains.
  A series needs >= 2 plain tiles between far and near head (:81); one bridge needs none. Unknown whether crossers 61/66 are >=2 apart.

## 3. Is series of singles on a band edge already supported?
- Structure and programs: yes, they already produce it (k bridges, k Over tokens, crossing order bug above).
- Cut search: no options for band edges (skips non-doubled, :119 `if (!bridge.doubled) continue`); the series is simply the only form.
- Placement: refused at :275 (size()!=1) and uses [0] at :275/:1419/:2566/heads_of. Chain floors for Heads->Heads exist (:81).
- So the smaller step is: lift the refusal and index bridges per Over token (token -> bridge by crossed edge; heads_of by k-th Heads). That makes
  series-of-1 work for all three failing hubs (the refusal is their first error; later errors unknown).
  "One bridge over k" is then a second step: a merge in the structure (join an edge's consecutive-token singles into crossed=[...]),
  Heads only at the first token, el.cross list for the S-bend column rule, a max_bridge_length check, and a cost choice in the cut
  (merge vs series), which would be the first non-doubled option in CutJunctionTrackBridges.
- Caveat: the cut search must not blow up: LLL_RRR hubs already take >100 s in the doubled compositions; D228 removing the cap makes it worse.
