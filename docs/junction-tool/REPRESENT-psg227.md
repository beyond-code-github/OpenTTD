# REPRESENT: can our graph model hold mfb's PSG 227 hub? (recon, 2026-11-14)

Read-only recon of `briefs/REPRESENT-psg227.md` (f0121e17fa). Sources: ANATOMY-psg227-bbh.md, DESIGN-modules §3-§4, D282-D286, the three CLAUDEs, `junction_topology.h/.cpp`, `junction_signals.h`, `junction_layout.h`, and the raw trace (`paths_all.txt`, `signals_bridges.txt`, `reach.txt`, `per_lane_events.txt`; one throwaway script in the scratchpad, trace read as text). Marked INFERRED where reasoned, not read. Line numbers are in the main tree at f0121e17fa. Nothing was built or run: path counts under our pairings are derived by reading `ExpandJunctionLanes`, not by running it.

## 0. Reading of the hub in our frame (used by every table)
- His arms: N (top), E (right), SW (box edge x=335, so it leaves west). E and SW are opposite edges, N is the third. INFERRED mapping: main line = SW(W side) to E, branch = N, then Mirror so the branch lands on the S side (`BuildJunctionTrackGraph` refuses non-S branch ports, topology.cpp:338). Any other main/branch assignment is equally good for this hub because it is symmetric: every arm has one fixed and one choice lane.
- Lane ids from the anatomy: out lanes N350 N353, E343 E342 (E343 inner), SW362 SW363 (SW362 inner); in lanes N354 N355, E344 E345, SW361 SW360 (SW361 inner). Choice lanes: N355, E345, SW361. Fixed: N354, E344, SW360. The hub drives on the left (in lanes are on the driver's left on all arms), which the road-side setting and Mirror cover.
- Our lane index on a slot (`JunctionPortSlot::lane`, topology.h:51) is 0 northmost/westmost on W/E and N/S sides. Which of his lanes is our lane 0 is therefore a mapping to fix after the mirror. INFERRED.

## 1. Summary (one table)
Counts are his elements. "As is" = the current type and rules hold it; "rule change" = the type can carry it but a rule or one new field must change; "not held" = no type member can carry it today.

| Element kind | Count in his hub | Held as is | Held with a rule change | Not held |
|---|---|---|---|---|
| Arms / lane groups (6 groups, LL_RR each, 3 arms) | 6 | 6 | 0 | 0 |
| Lane paths (reach table) | 18 | 12 (the turning ones, inside AnyToAny's 16) | 6 (W-E and E-W: not lane i to lane i) | 0 |
| Direction splits (5 clean + E345's doubling-first one) | 6 | 5 | 0 | 1 (E345: doubling, peel, 4-in-2-out cross) |
| Line-choice splits (per (input line, destination)) | 6 | 0 | 6 (late split on a 2-path trunk; sits in a band, tree built per slot) | 0 |
| Joins (output lane merges, 3 sources each) | 6 | 0 | 0 | 6 (merge order choice-first; two-stage and three-on-a-tile shapes; prio side) |
| Doublings of fixed/choice tracks (rejoin as a pair) | 5 sites, about 9 rejoin blocks | 0 | 5 (as our doubled leg, off a row) | 4 (the rejoins that merge three tracks of one path, or two-way gated) |
| Diagonal crosses (4 in, 2 out; merge then split) | 3 (E345 direction cross, N355 line cross, and the SW-input cross at 367-368,346-347 counted once; INFERRED) | 0 | 0 | 3 (D285 leaves them out; D24 strict) |
| Bridges | 19 | 4 (span 2, 2 crossers, within cap) | 15 (span 3-4: over the series cap of 2, bundles of parallel single bridges) | 0 |
| Tunnel | 1 | 0 | 1 (surface tracks above only; ours: single separation, hover never chooses) | 0 |
| Bridge pair that is a parallelogram doubled bridge (375,346 / 376,345) | 1 pair (2 of the 19) | 1 pair (D80 shape) | 0 | 0 |
| Prio styles | 3 | 0 | 0 | 3 (no per-token signal type, no two-way pieces, prio on the fixed lane and on branch-out merges not modelled) |
| Choice-input entry signals | 18 entry | 0 | 0 | 18 |
| Rows / bands (through lane on a row) | 4 main lanes | 0 | 0 | 4 (his through paths are serpentines with lane changes; our rows assume straight lane rows, "a through edge off its row" is refused) |

The headline: lane paths, arms, bridges and the doubled-bridge shape are expressible; joins, signals and the row/order assumption are not. Details and file:line below.

## 2. Q1 Topology and lane paths

### 2.1 His hub as our topology
| Element | His tiles | Our type / member | Verdict | Why |
|---|---|---|---|---|
| Main line W (SW arm) | in SW360 SW361 / out SW362 SW363, edge x=335 | `JunctionLaneGroup` side W, lanes 2, main, two groups (e-in, w-out) (topology.h:31) | as is | LL_RR on both sides; groups in slot order (topology.h:60) |
| Main line E | in E344 E345 / out E342 E343, edge x=392 | groups side E, main (topology.h:31) | as is | same |
| Branch N, mirrored to S | in N354 N355 / out N350 N353, edge y=311 | groups side S, `main=false`, lanes 2 each; `MakeJunctionHubTopology` (topology.h:394; .cpp:1765) | as is | LL_RR branch is the LL² case |
| Movements | 6 in-out pairs, all turning | `JunctionTopology::movements` (topology.h:47) | as is, with the next row | MakeJunctionHubTopology lists every in group to every out group on another side; so W-in to E-out is the through movement (both main, same line) and the four N movements are exits and joins |
| Notation spacing | E: gap 0 (LLRR); N arm outer lanes 3 apart at the edge (350 and 353) and approach diagonally; SW: gap 0 | `JunctionLineSpec` gap per line (spec: "spacing may vary along a line"); arms hold the notation only at the ports | as is, INFERRED | the N arm's two out lanes are not adjacent at the box edge in his save (the notation grammar says tracks of one direction are adjacent, topology CLAUDE). Whether the 3-tile spread is arm geometry (a Fan or S-bend inside our box) or a notation `L__L` we cannot parse is open (Q1 below) |
| Port offset between W and E | W lanes at y 360-363, E lanes at y 342-345 (an 18-tile step) | `Fan` step between port row and stack row (topology.h:208) | open | does placement let the E ports sit at other rows than the W ports? INFERRED yes (fan sized by the solve), not read |

### 2.2 The 18 lane paths and our ExpandJunctionLanes (topology.cpp:52)
Rule recap: a through movement (both groups main, same line) is lane i to lane i and refuses unequal counts (.cpp:73-80); a turning movement with more than one lane on both sides goes by `pairing`: AnyToAny every pair, LaneKeepsLane k to k, LanePerDirection round-robin (.cpp:82-118; enum topology.h:40).

| Movement | His paths (from reach table) | AnyToAny | LaneKeepsLane | LanePerDirection | Verdict |
|---|---|---|---|---|---|
| SW to E (through) | SW360 to E343; SW361 to E342, E343 (3) | 2 (lane i to i) | 2 | 2 | rule change: 3 paths, one lane changes row (SW361 reaches both E lanes). The through rule is not a pairing option today |
| E to SW (through) | E344 to SW363; E345 to SW362, SW363 (3) | 2 | 2 | 2 | same |
| N to E | N354 to E342; N355 to E342, E343 (3) | 4 | 2 | 4 (n=1 turning movement for the main out group, all lanes) INFERRED | new pairing: fixed lane to one out lane, choice lane to both |
| N to SW | N354 to SW362; N355 to SW362, SW363 (3) | 4 | 2 | 4 INFERRED | same |
| E to N | E344 to N353; E345 to N350, N353 (3) | 4 | 2 | 4 | same |
| SW to N | SW360 to N350; SW361 to N350, N353 (3) | 4 | 2 | 4 | same |
| Total | 18 | 20 | 12 | 16 | AnyToAny contains every one of his 12 turning paths and adds 4; none of the three gives his 18 |

What a new pairing must produce (call it per-line choice):
1. Per in group, one choice lane (reaches both lanes of every destination group) and one fixed lane (one lane per destination group). His choice lane per arm: N355 (outer in lane), E345 (outer), SW361 (inner): not a single "outer/inner" rule, so it is per-group data. ANATOMY already notes the E reading may differ (open).
2. The fixed lane's out lane per destination, from his table: N354 to E342 and SW362; E344 to N353 and SW363; SW360 to N350 and E343. Constraint visible in the data: for every destination arm, the two other arms' fixed lanes land on different lanes of it, so each out lane has exactly one fixed source (N350 SW360, N353 E344, E342 N354, E343 SW360, SW362 N354, SW363 E344; E343 and N350 share SW360 as source but are different arms). The choice lanes cover the remaining two sources per out lane.
3. The through movement must follow the same pairing, not lane i to i: the W-E pairs are "SW360 fixed to E343, SW361 choice to both" like any other pair. This means the through-movement branch of `ExpandJunctionLanes` (.cpp:73) must defer to the pairing when the pairing is per-line choice, or the pairing must carry an explicit pair list.
4. Each out lane then has three sources, two choice and one fixed; this is needed by D283 and by every join below.

## 3. Q2 Track graph (splits, merges, joins)
Our graph: per in slot a split tree (first by destination group, then by slot), per out slot a merge tree mirrored (first by source group, then by slot), one `Leg` edge per path in between, two-way nodes, cascade for more than two keys (`JunctionTreeBuilder`, topology.cpp:235-329; `JunctionTrackNode` topology.h:97, `JunctionTrackEdge` topology.h:105). Splits come before merges along every track.

| Element | His tiles and lane paths | Our type / member | Verdict | Why |
|---|---|---|---|---|
| Direction split N355 | 362-363,324-326; N355 to {E pair} \| {SW pair} | split node on in slot, keys by group (`Keys`, .cpp:249) | as is | exactly the first split of the tree |
| Direction split N354 | 362-364,328-329 | same | as is | |
| Direction split E344 | 379-380,344 | same | as is | |
| Direction split SW360 | 356-357,339-340 | same | as is | |
| Direction split SW361 | 363,354 | same | as is | |
| E345 direction split | doubling at 381-382,346 then peels 374-379,345 and 374-378,346, then a 4-in 2-out diagonal cross 372-373,345-346 | none: a node is two legs; the cross merges then splits | not held | merge-then-split is D24, which D285 keeps strict (first build leaves these out); doubling first is not a split. ANATOMY (a) says it still completes a direction split |
| Line split N355 to E | 369-370,330-331; N355 (E342\|E343) | split node, second level of the tree (keys by slot); non-main in slot so its level is the band next to the destination rows (`level = 2*(1+largest)`, .cpp:395-410) | rule change | holds as a late split in principle (the 2-path trunk edge before it may carry the crossing: crossings are assigned to the biggest edge pairs, .cpp:460); but the tree is built straight after the direction split so any other "after the core" position is a placement choice, not a node property. INFERRED |
| Line split N355 to SW | cross 367-368,346-347 | split node + crossing | rule change | his version is a diagonal cross fed by two parallel tracks; ours would be a split node, then two legs (see doubling rows) |
| Line split E345 to N | 361-362,340 (path signal before it) | split node, `branch_only` merge/split level 2r+2 (band under lane r, .cpp:372-380) | rule change | main in slot toward a branch out group sits in the band beside its own row, near the port, not mid-core |
| Line split E345 to SW | 370-372,347-348 | same | rule change | same |
| Line split SW361 to E | 367-368,337 | split on a main-row through path | rule change | a main in lane splitting toward a main out group is on its row (.cpp:365) |
| Line split SW361 to N | 359-361,342-343 | as E345 to N | rule change | |
| Doubling N354 to SW (365,331 split; rejoin 365-366,346-347) | one path, two parallel tracks, rejoin as a pair | `copies` of a doubled edge (`JunctionTrackPrograms::copies`, topology.h:243), `LaneSplit`/`LaneMerge` roles | rule change | our copy exists only on a row edge that a crossing doubles (structure, .cpp:880-925); his doublings are on legs off any row |
| Doubling N355 (364-365,330; 366-367,331) | same | same | rule change | |
| Doubling E344 to N (363-364,340-341 split; rejoin 362-363,330-331 with the third E344 input) | the rejoin takes the pair plus a third input | none | not held | a three-track rejoin of one path is not a node (nodes have two legs) |
| Doubling SW360 to N (357,334; rejoin 357,335-336) | two-way exit/exit piece gates the split | doubled edge + two-way signal | not held for the signal half | two-way pieces: see Q4 |
| Doubling SW360 to E (374-376,340-342; 373-374,338-340) | rejoin pair | doubled edge | rule change | |
| Join N353 (3 inputs on tile pair 360,328-329) | E344 fixed + SW361 + E345 | merge tree: keys by source group first: E344 and E345 share group E, so ours is (E344+E345) then with SW361; node has two legs (topology.h:97-110) | not held | his merge order is choice+choice then fixed (below); three legs on one tile pair is not a node shape. The placement rule "only a node's serving bend shares its point" (placement CLAUDE) forbids two merge points on one tile pair; between two merges a legal signal piece is also required (signals CLAUDE: BeforeMerge, no back-to-back) |
| Join SW362 (3 on 367,356-357) | N354 fixed + N355 + E345 | same | not held | same; and N354 and N355 are one source group |
| Join SW363 (3 on 370,358-359) | E344 fixed + E345 + N355 | same | not held | same |
| Join N350, two stages (358,330-331 then 356,328-329) | SW361+E345 then +SW360 fixed; two signals fit between | two merges in tree order (SW360+SW361) then +E345 | not held | merge order differs (ours pairs the same-arm lanes first); for N (a branch out) the merge level is the band `2*rows` (.cpp:395) and no row joiner exists, so no prio element is produced |
| Join E342 (378,338 then 378-379,339) | N355+SW361 then +N354 fixed | out slot on main row: N354+N355 merge first as one joiner (D224), joins SW361's row | not held | his first merge is a branch lane with a main lane, ours a branch pair; then a second joiner on the same row, refused: "more than one joiner on a lane is not ruled yet" (.cpp:877) |
| Join E343 (375-376,343 then 377-378,342-343) | N355+SW361 then +SW360 | sources SW360 and SW361 are both main rows | not held | two main rows merge into one main lane: ours only has through lane i to i; refused or unreachable (rows: "a main slot without a through partner", .cpp:165; "two turning legs from one main row", .cpp:963). INFERRED which refusal fires |
| Three inputs on one tile pair (N353, SW362, SW363) | shown above | none | not held | |
| Merge order inside each join | choice lanes first (no prio between them), then the fixed lane | `Keys` sorts by source group then by distance (.cpp:249-270); the order is fixed by group, not settable | not held | needs an order that is not group-first; this is what "choice before fixed" means in the graph |
| Parallel crossing of a pair merged before the crossing (SW to E "pass under the bridge as one"; E to N "over then join") | ANATOMY clarification; (e) fails for SW to E in the trace | an In or Out edge carrying 2 paths crosses as one edge (crossings on the biggest edges, .cpp:460) | as is | the model already lets a trunk carry several paths; only the merge-before-crossing for SW360+SW361 does not exist in his hub (rule (e) fails), so nothing to hold |

Edge-level summary (lane paths carried per edge): in our graph every path ends in a one-path `Leg`. His split-late tracks carry two paths over the core (choice lane after the direction split: N355 to E carries N355-E342 and N355-E343) and then line-split. The edge that crosses is the In-tree edge between the direction split and the line split, which our crossing assignment already prefers (biggest edges). So the late split is expressible; the early/late position of the line split relative to the core is then decided by which edge carries the crossing, and by the node's level, which our builder fixes from the slot (main on its row or band under it; branch in the band by the destination rows). INFERRED, would need a graph dump to confirm.

## 4. Q3 Crossings, bridges and the tunnel
Our rules (topology CLAUDE and structure code): every main-line bridge is doubled (a crossed row edge carries one doubled bridge over all its crossings); a branch bridge is single; two non-row edges: joiner over turning leg, of two joiners the farther from the main line over, of two turning legs the one from the far side over, through off its row refused (.cpp:925-963); a bridge packs its crossers exactly and the heads hug the outermost; series cap `JUNCTION_BRIDGE_MAX_CROSSERS` = 2 crossers per bridge (topology.h:181), longer sets are cut into series bridges with one signalled tile between; tunnel only on a single separation, `tunnel` on `JunctionTrackBridge` (topology.h:184), hover never chooses one; bridge middle limit `max_bridge_length`.

The crossers below are the span tiles (heads hug crossers); a script over the 18 shortest paths gave a lower bound per bridge (tiles with a path under it); the full count needs the whole save. "Over" = shortest paths that use the bridge, 0 means only non-shortest paths do. All 19 heads are single bridges (only the 375/376 pair has doubled-bridge geometry).

| Bridge (heads) | Dir | Span | Crossers (span; lower bound from shortest paths) | Over paths (shortest) | Our rule | Verdict |
|---|---|---|---|---|---|---|
| 358,336-340 | Y | 3 | 3; 3 tiles hit (337-339) | 1 | cap 2 cuts to 2+1 in series; his one bridge | rule change |
| 359,330-333 | Y | 2 | 2; 2 | 1 | within cap | as is |
| 359,335-339 | Y | 3 | 3; 3 | 1 | cap | rule change |
| 360,334-338 | Y | 3 | 3; 2 | 1 | cap | rule change |
| 361,334-338 | Y | 3 | 3; 2 | 1 | cap | rule change |
| 362,334-338 | Y | 3 | 3; 2 | 1 | cap | rule change |
| 363,334-338 | Y | 3 | 3; 2 | 0 | cap | rule change |
| 365,333-338 | Y | 4 | 4; 4 | 0 | cap | rule change |
| 366,333-338 | Y | 4 | 4; 3 | 1 | cap | rule change |
| 367,334-338 | Y | 3 | 3; 3 | 1 | cap | rule change |
| 368,334-338 | Y | 3 | 3; 3 | 1 | cap | rule change |
| 365,340-344 | Y | 3 | 3; 2 | 0 | cap | rule change |
| 366,340-344 | Y | 3 | 3; 2 | 1 | cap | rule change |
| 367,340-344 | Y | 3 | 3; 2 | 1 | cap | rule change |
| 368,340-344 | Y | 3 | 3; 2 | 1 | cap | rule change |
| 369,348-351 | Y | 2 | 2; 2 | 1 | within cap | as is |
| 372,334-339 | Y | 4 | 4; 4 | 1 | cap | rule change |
| 375,346-378,346 | X | 2 | 2; 2 | 2 | with 376,345-379,345 the doubled bridge (copy 1's heads one tile upstream, equal spans) | as is |
| 376,345-379,345 | X | 2 | 2; 2 | 2 | same | as is |
| Tunnel 371,338-375,338 | X | 3 under | 3 surface tiles (372-374) | 1 | `Tunnel`/`Above` tokens, single separation (topology.h:210-216) | rule change |

Stacks (bundles). Tracks of one stack run side by side in adjacent columns, each over the same crossers:
- Stack A: x358-363, y334-340, 6 bridges, heads staggered one tile (358: 336-340; 359: 335-339; 360-363: 334-338). Crossers about 3 tracks.
- Stack B1: x365-368, y333-338: 4 bridges, spans 4,4,3,3 (heads 333,333,334,334).
- Stack B2: x365-368, y340-344: 4 bridges, span 3.
- Singles: 359,330-333; 369,348-351; 372,334-339; the tunnel.
- Pair D: the doubled bridge.

Differences from our rules:
1. Series cap: 15 of 19 bridges have more than 2 crossers in one bridge (12 of span 3, 3 of span 4). Our cap would cut each into series bridges plus a signalled tile between; his tiles do not allow it (adjacent crossers, no tile to spare). The cap is "provisional until the D208 rig experiment" (topology.h:180); his hub says 3-4 crossers per bridge occur in the corpus (INFERRED that it is built and works; the save is a real game).
2. Bundles: ours has no unit of "parallel bridges of different edges over the same crossers"; each edge has its own bridge and each pair of edges its crossings. Four parallel bridges of four different edges over three crossers is 12 edge pairs each with one crossing; representable as individual band crossings if the order places them adjacent (INFERRED), but D286's "bundle as one unit" is exactly what is not in the graph yet.
3. Staggered heads in a stack (heads one tile apart): ours hugs the outermost crosser of each bridge, which gives staggered heads only if the crossers are diagonal; INFERRED consistent.
4. Over and under: the trace shows 1 shortest path over most bridges; which side is over cannot be decided from the 18 shortest paths for the bridges with 0 or 2 overs. Our rule "through goes over, joiner over turning leg" cannot be checked per bridge without the edge labelling (his paths have no main row); INFERRED they would pick the same side for joiners and differently for lane-changing paths that our model has no through version of.
5. Doubled: ours doubles every main-line bridge; his bridges over/under main-line lanes are single (only pair D is doubled). A row-edge with a crossing would force a doubled bridge on his through lane where he built a single. Refuse or make different.
6. Tunnel: single separation only, surface crosser above; his has a bridge-like 3-tile span with 3 surface tiles and a tunnel next to the E-route two-way chain. Holds in the model (Tunnel and Above tokens) but the hover never picks one and the rule families "bridge-only" apply (D281 says the bridge-over-S-bend row still applied with tunnels).
7. Limits: bridge type needs `ChooseJunctionBridgeType`; bridge_type 1 and 10 (head heights 1 to 3) fit any span up to `max_bridge_middle` = 10; his longest middle is 4. Fine.

## 5. Q4 Signals and prios
Our tokens: `JunctionSignalReason` { BeforeSplit, AfterSplit, BeforeMerge, AfterMerge, BeforeBridge, AfterBridge, SeriesGap, Stub, Bay, Fill, Prio } (signals.h:31), `JunctionTrackSignal` has route, reason, tile, one `facing` (signals.h:36). The layout's `JunctionSignal` has `type` (SignalType) and `variant` (layout.h:36), so the data carries a type, but the rules set it from the spec's one setting (Block or PathOneWay, spec.h:45); prio elements place their own pre-signals (combo/exit/entry, signals CLAUDE and `junction_signal_pass`). A prio is a parallel unused track joined upstream of a merge on a main-line lane (structure Approach/Prio items, topology.h:167).

| His element | Count | Our expression | Verdict | Why |
|---|---|---|---|---|
| Prio style 1: single three-input join; fixed lane gated by a two-way combo (361,329r; 366,356l; 371,358u), both choice inputs by entry signals | 3 joins | none | not held | three-input join node (Q2); entry on the choice inputs and a two-way combo on the fixed lane are not tokens; the pre-signals we use are inside the prio element only |
| Prio style 2: two-stage join; choice lanes block/block, merged track passes an entry signal, fixed lane on a plain block signal | 3 joins (N350, E342, E343) | none | not held | cascaded joins exist (two-way nodes), but the entry signal sits on a piece between the merges (BeforeMerge of the second); type per token missing; prio side reversed |
| Prio style 3: path signals at a line split or doubling (361,340l, 363,340l), two-way exit/exit gating the doubled rejoin (362,332y, 363,332y) and doubling (357,334-335), two-way combo/exit chain on the E-route (372,336r, 372,338l, 374,338r) | 2 path signals, 11 two-way pieces | `SignalType::PathOneWay` only as the global default; no two-way piece | not held | no per-token signal type; no two-way piece in `JunctionTrackSignal`; ANATOMY marks the roles INFERRED |
| Choice-input entry signals | 18 entry | `Prio` reason inside a prio element only | not held | needs "entry on a choice input of a join" as a token with a type |
| Prio direction | fixed lane has the prio, choice lanes yield (D282) | our prio element: the main-line lane has the prio, the joiner yields (signals CLAUDE; `Prio` only for "a merge onto the main line" with a joiner) | not held | reversed owner; and it must also apply at merges into a branch out lane (N350 here), where we produce no prio element (no row joiner) |
| Signal spacing (1-tile gaps 0 to 4 per path, 55-70 percent at 2 tiles, up to 9 over a bridge/tunnel) | all paths | Bay, SeriesGap, Fill, and the SD spacing; "no back-to-back signals where one suffices" | unknown | the checker may reject some 1-tile gaps (ANATOMY suspects TL violations); to be measured with the corpus oracle, not argued here |

## 6. Q5 Order and structure
Our order: rows are the W-side main lanes (4 here), the through lane of each lane sits on its row (`JunctionTrackRowItem` rows, topology.h:150), bands between rows hold the other edges' nodes and crossings, stack items per row are Lane, Prio, Copy, PrioAndCopy, Approach (topology.h:167). Only what blocks representation:

| Place in our layout | His layout | Blocks? | Why |
|---|---|---|---|
| Row of each main lane (4 rows) | rows come from W-side lanes; E slots take the row of their first through partner (`JunctionSlotRows`, .cpp:145-170) | blocks | his through paths are not straight (W-E paths run 94-104 tiles across a 57-wide box) and change lane; our row = the lane's straight run. E343 has two W partners (SW360, SW361) so its row is the first found by path order, and the other path is a lane change, which has no row edge (INFERRED) |
| Through edge crossing off its row | his through tracks (SW to E) cross the bundles of the other flows in the middle of the core | blocks | "a crossing of a through edge off its row is not ruled yet" (.cpp:946) |
| Rows of the branch (N to S) | branch is the S side, bands sit between rows | no | the branch lanes are not rows |
| Which of his tracks would be rows | SW360 to E343 and SW361 to E342 (the lane i to i W-E paths) would be rows R2 and R3 under the mirror; his other four through lanes are not rows | blocks | only the i to i through pairs map; a W-E path with a lane change is not a row edge |
| More than one joiner on a lane | E342 and the other two-stage joins | blocks | refused at .cpp:877 |
| Two turning legs from one main row / two joiners from one branch slot at one level | N355 and N354 both joining E342, E343 etc. | blocks | refused at .cpp:963 and .cpp:957 |
| Order sweep gathers a band node from its legs into one item; legs must be adjacent | his 3-input joins: three legs to one tile pair | blocks | node has two legs |
| Band crossings among non-row edges | his bundle crossings (parallel stacks A, B1, B2) | no, in principle | the band sweep handles any adjacent crossing set (`JunctionTrackBandEvent`, topology.h:125); size of the stack is a placement question |
| Rows and bands stay the core generator's internals (DESIGN-modules §5.5) | D286: the core is a bundle T, tracks two wide | no new block, but the bundle is not in the graph | the structure has no "bundle" element (parallel tracks that cross as one); each member would be an edge |

## 7. Ordered list of model changes (smallest first)
Each says what must become expressible, no design.

1. **Per-line-choice lane pairing as a topology option**, with the through movement following it (explicit pair list per movement, fixed lane and choice lane per in group, the out lane the fixed lane takes). Unblocks: all 18 lane paths (the 6 W-E paths and the 12 turning ones without AnyToAny's 4 extras), the reach table, D283, the Q1 rows.
2. **Merge order that is not source-group-first**: a join of three sources whose order is set by the pattern (choice lanes first, then the fixed lane), and splits likewise if wanted. Unblocks: the merge order inside all 6 joins; the group keys in `JunctionTreeBuilder::Keys` currently force N354+N355 (and SW360+SW361, E344+E345) together first.
3. **Per-token signal type on a rule token** (entry on a choice input, combo or block on the fixed input, exit), instead of only the spec's one type; and the prio owner chosen by the pattern (fixed lane has it, not the main line). Unblocks: prio styles 1 and 2 (6 joins), the 18 entry signals, prios at merges into a branch out lane.
4. **Two-way signal pieces and per-token path signals** as tokens (a piece with signals in both directions; a path signal at a split or doubling). Unblocks: prio style 3 (11 two-way pieces, 2 path signals).
5. **Series cap larger than 2 (3 and 4 crossers in one bridge) at least where tiles do not allow a signalled tile between**, and a bridge span of 4 when the crossers are packed. Unblocks: 15 of 19 bridges. The cap is provisional (D209).
6. **A through lane that changes row, and a lane-changing path as a row edge or a refusal-free edge**: W-E paths whose in and out lanes do not share a row; E343 with two main-row sources. Unblocks: the 6 through paths in the order, the rows (4 rows), the E343 and N350 joins, the through-off-row crossings (.cpp:946).
7. **More than one joiner on one main lane, and two turning legs from one main row / two joiners from one branch slot at one level** (.cpp:877, :957, :963). Unblocks: E342 and E343 joins, N355/N354 joins.
8. **Doubling (a doubled bridge or copy pair) on edges that are not a row edge**, with the same path on both copies and a rejoin as a pair or a trio. Unblocks: the 5 doubling sites and their rejoins, "prio over a doubled bridge".
9. **A bundle as a unit** (parallel adjacent single bridges of different edges over the same crossers, stacks A, B1, B2) in the graph, not only per edge pair. Unblocks: the 15 stack bridges' placement, D286's core.
10. **A three-leg join node (or two merges on adjacent tiles with no signal piece between)**, and the signal rule exemption that goes with it. Unblocks: N353, SW362, SW363 and the cascades' tight joints (the unsignalled tile pairs).
11. **Merge then split (diagonal cross, 4 in 2 out)**, reopening D24 and D285. Unblocks: the 3 diagonal crosses and the E345 direction split.
12. **Tunnel in the hover and the bridge-only rule families skipped** (design step 1 already). Unblocks: the tunnel.

Cheapest steps first: 1 to 4 are data and token additions on the existing graph; 5 is a constant; 6 and 7 lift refusals; 8 to 11 change node/edge shape.

## 8. Open questions
1. Which assignment of W/E/N to main and branch is wanted, and is the E arm's "innermost choice" (his words) the choice lane E345 (outer by the box-edge) measured at the core? (ANATOMY open.)
2. Is the N arm's 3-tile spread between N350 and N353 at the box edge an arm S-bend inside the box, or a notation we cannot parse (`L__L`)? Our notation requires same-direction tracks adjacent.
3. Do the W and E ports of the main line have to be at fixed rows, or does a Fan sized by the solve allow an 18-tile step like his?
4. Crossers per bridge: the 3-4 crossers per bridge are INFERRED from spans. Run a tile-by-tile read of `bbh.json` to confirm, and list which edge is over at each bridge (needs all paths, not the 18 shortest).
5. D285 leaves the diagonal crosses out; does the first LL² keep the E345 direction split as direction split then line split in two nodes (D24) and accept the lost compactness?
6. Does the checker accept his 1-tile signal spacings and the tile pairs with no signal between merges? (ANATOMY "TL violations" suspicion; needs the oracle.)
7. Is a 3-input join one node or an accepted cascade for the first build (D283 says "three-input joins")?
