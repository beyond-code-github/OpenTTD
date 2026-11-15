# RECON: four decks in the width-2 core (H1c)

Read-only recon at HEAD 0559eb114d. All paths under src/junction/. "INFERRED" marks anything not verified in code.

## Key finding up front

At width 2, "copy" already means "track" everywhere (programs.copies[e] = track 1, chain_copy[e] = track 1's chain, JunctionTrackRoute::copy = track, bridges[b][1] = track 1's heads). At width 1 it means "the doubled leg's second lane". The two meanings share one index, so a 4-way (track x copy) needs a new index, not a reuse. Every consumer picks the width by `bundle` or `two_chains`, not by a stored field.

## Q1. Storage (index meaning, reader counts)

| Holder | Width 1 | Width 2 | Sites |
|---|---|---|---|
| `JunctionTrackPrograms::copies` (topology.h:280, "doubled edge's copy 1, or track 1 of every edge of a bundle graph") | copy 1 of a doubled leg/in/out edge | track 1 of EVERY edge (band edges too, topology.cpp:1289) | writers topology.cpp:1224,1234,1250-1251,1264-1265,1278-1279,1289; readers placement.cpp 90,291,325-326,893,896,965,982,1104,1293,2420 (roughly 12), render.cpp 106,187, signals.cpp 49,63, hub.cpp 361-367, legacy/junction_nodes.cpp 735,743 (about 10 more). About 30 total. |
| `JunctionIsDoubledEdge` (topology.cpp:241) = `!copies[e].empty()` | true on doubled edges | true on every edge (so it is not a "doubled" test at width 2) | callers: placement.cpp:149 (`IsDoubledEdge`) and `JunctionEdgeTracks` (topology.cpp:246). |
| `JunctionEdgeTracks` (topology.cpp:246) = `width==2 \|\| doubled ? 2 : 1` | 2 for doubled | 2 | placement.cpp:237 (bundle port-count check) only (INFERRED: few other users). Becomes 4 on a crossed bundle row edge. |
| placement `chain_copy` (placement.cpp:281) + `two_chains(e)` (:282) | copy 1 chain of a doubled row leg: SBend (LaneSplit) then HeadsPair | track 1's chain, row edge and band edge | `chain_copy` about 60 references; `two_chains` about 15 (1046, 1095, 1292, 1585, 2173, 2401, 2508, 2530, 2781, 2829, 2892, 3746...). Idiom everywhere: `for (elems : {&chain[e], &chain_copy[e]})` or `c == 0 ? chain[e] : chain_copy[e]` (binary choice, hard-coded). |
| `nth_heads(elems,k)` (:284) | k-th HeadsPair of a chain | same | about 14 calls (2177, 2421-2422, 2503, 2509, 2533, 3667...). Takes a chain, so it generalises for free. `lane_bend` (:289) compares `&elems == &chain[e]` to pick programs.edges vs copies: breaks with more chains. |
| `JunctionTrackPlacement::copies` (placement.h:286) | spans of copy-1 tokens | spans of track-1 tokens | built :3714-3749; read render.cpp:107. |
| `JunctionTrackPlacement::bridges` (placement.h:287) `vector<array<JunctionBridge,2>>` | [b][0], [b][1] = the two copies' heads of a doubled bridge | [b][track] when `bundle && two_chains(edge)` | writers :3799,3818; readers render.cpp:118, legacy/junction_nodes.cpp:795. Fixed array size 2: must become size 4 or a vector. |
| `JunctionTrackRoute::copy` (route.h:~70) | 0/1 | track | readers: signals.cpp 52-56, 72, 77; hub.cpp 153, 397, 402; block.cpp 1049-1078; placement sym_routes (`SymRoute::copy`, :1176); legacy about 8. About 25. |
| `JunctionTrackPieceTag::copy` (placement.h, `uint8_t`) | 0/1 | track | render.cpp 66, 222; pipeline.cpp 252, 265, 386 (edit keys); legacy/junction_nodes.cpp 735-889, 2435-2576 (detour edits keyed by copy). About 15 sites. |
| `JunctionTrackStep::copy` (topology.h:253) | `Under` only: which copy of a doubled bridge is overhead | same (band edges' Under at topology.cpp:1338-1339 emit both copies) | Under only. |
| `RowItems` | `.copy` one Copy/PrioAndCopy item | `.copy` = track 1's item, `.lane` = track 0's | see Q2 |

Render: `copy1_of` (render.cpp:68), `c < (has_doubled ? 2 : 1)` (:67). Sym routes: `c < (has_doubled ? 2 : 1)` (placement.cpp:1095-1096).

## Q2. Structure (stack items)

Width 1 (topology.cpp:963-1000, "The row's per-edge needs", :977+): `crossed` = any crossed row edge starting on this row (:966-968). `need_copy[side]`: crossed puts the Copy opposite the joiner, else north (:988). `side_item` (:989-1000) emits per side, in order north, Lane, south: `PrioAndCopy` > `Prio` > `Copy` > `Approach`. So the stack of one row is [north-side item?, Lane, south-side item?].

Width 2 (:957-975): fixed four items, ordered by `east[r]`: `lanes[2] = {Lane track 0, Copy track 1}`, `sides[2] = {BlockSide 0, BlockSide 1}`. First-pair order flips with `east[r]` so track 1 lies on the diverging side. It never consults `crossed`.

A per-track doubling copy item needs: kind `Copy` (or a new `TrackCopy`), `track` field set to the track it doubles (the field already exists: `JunctionTrackStackItem::track`, "bundle track the item carries"), `row`. Order rule, outward: across travel [t0 copy, t0 Lane, t1 Copy-slot (today's Copy), t1 copy]; i.e. stack row = {TrackCopy 0, Lane 0, Lane-as-Copy 1, TrackCopy 1}, mirrored by `east[r]`, only when the row has a crossed edge (`crossed` test copied from :966). BlockSide items stay after. Placement `item_pt` rows depend only on stack order (INFERRED), so extra items are free for rows.

Readers of RowItems (topology.h:372; built by `FindRowItems`, topology.cpp:189-203): `.copy` is a single index overwritten by the last Copy item (:200), so it cannot hold two. `.block_side[track]` exists per track and is the model for `.track_copy[track]`. Readers: topology.cpp 1231 (bundle row program: `item[2] = {lane, copy}`), 1255, 1272 (width-1 doubled in/out), 1139+; placement.cpp 934 (`row_items[r].lane / .copy` for HeadsPair index at bundle), others in 1000-1010 area (INFERRED, grep `row_items`). `.approach/.north` unaffected.

## Q3. Programs

Width 1 doubled leg (topology.cpp:1214-1286):
- Leg (`e == leg_of[r]`, :1239-1253): copy 0 gets `Over...` then `SBend(LaneMerge, hand, lane item)`; copy 1 gets `SBend(LaneSplit)` then `Over...` (lines 1244-1251).
- Doubled in edge (:1254-1266): `Fan(a)`, Over..., `SBend(LaneMerge, b)`; copy 1 gets `SBend(LaneSplit, b)`, Over...
- Doubled out edge (:1269-1280): Over..., `SBend(LaneMerge, c)`, `Fan(c)`; copy 1 `SBend(LaneSplit, c)`, Over...
- No HeadsPair at program level; HeadsPair is a placement Elem (:930) built from the Over token whose `crossed[0]` starts a bridge.

Width 2 row edge (:1226-1237): both tracks get the SAME shape, `Fan(item[c])` at a port end, `Over x` for every crossing; track c's item is `row_items.lane` / `.copy`. Track 1 is `programs.copies[e]`. No SBends at all. So a crossed bundle row edge currently has two decks (parallelogram pair), not four.

Needed: per track, today's width-1 leg shape: `SBend(LaneSplit)` + Over... on the track's copy program, `Over...` + `SBend(LaneMerge)` on the track's own program. Roles LaneSplit/LaneMerge reuse; `index` = the track's Lane/Copy item vs the outward copy item.

## Q4. Placement

Doubled-leg block: placement.cpp:2413 `if (is_row_edge(e) && doubled_edge(e) && !bundle)` ... through :2488 `continue`. Relations: copy-1 lane split S-bend starts on the exit item and ends on its token's item (`eq` :2430-2431); heads on their item rows; lane-split run floor (`FloorKind::LaneSplit`), head stagger `along(...,1,true)` copy 0 one past copy 1 (D118) per bridge b (:2447-2448); lane merge one past copy 0's far head (:2454); prio element floor after the merge; crossers' tiles between heads on both copy row and lane row ("hug", :2463-2486, `hugs.push_back`).

Bundle row-edge block (:2490-2535, `bundle && is_row_edge && chain_no == 0`): hugs on each built chain via `edge_tiles_all(x)` for pass in 0..1 (`two_chains`), and "bridge pair stagger" `along(heading, first_heads.first, second_heads.first, 1, true)` (track 1's heads one upstream). Both loops hard-code 2 chains.

HeadsPair: Elem built at :925-934; `el.index = row_item or (bundle ? c==0 ? lane : copy ...)`. With SBends in a bundle chain `row_item` becomes the SBend's item (works unchanged since `row_item = step.index`). The `bundle` ternary at :934 only fires with no earlier SBend.

`sym_routes` (:1078-1180): one SymRoute per path and copy, `c < (has_doubled ? 2 : 1)` with `has_doubled = any_of(edges, two_chains)`, start point `track_port_pt` for `bundle && c == 1`. Equal pathfinder cost for doubled legs (both copies same bends) is therefore keyed to c in {0,1}; the 4-way needs the route key (edge, track, copy) and the same start-point rule. (Equal cost is arranged by the same-shape programs; INFERRED that no explicit cost equality code exists beyond that.)

Occupancy `related` (:1449): same `edge` and `a.copy == b.copy || either < 0`. For 4 chains: copies of different tracks of one edge are unrelated. That is correct as intended (track 0 and its copy rejoin, track 0 and track 1 do not), but `copy` must then be a flat chain id (track*2+copy) or the `Occupant` needs a track field; and the "part both share" -1 at width 1 (:1437) must stay.

With 4 chains per edge: every `{&chain[e], &chain_copy[e]}` loop (about 15: 780, 813, 1461, 2160, 2955, 3444, 3503, 3543, 3568, 3611...) must iterate chains[e][0..3]; ordering at :2053-2055 ("south ? a.copy > b.copy : a.copy < b.copy", the item-row occupant order) needs outward order [t0 copy, t0, t1, t1 copy].

## Q5. Render and signals

Render (render.cpp:66-118): per (edge at bundle, c in {0,1}) with `copy1_of(e) = c==1 && (bundle || doubled_leg(e))`; picks `programs.copies/edges`, `placement.copies/edges`, `placement.bridges[b][copy1_of ? c : 0]`. A 4-chain route is one route per (edge, track, copy): `c` loop to 4, `copy1_of` becomes "which program list", tags carry a flat id.

Signals (signals.cpp:47-80, `JunctionRouteEvents`): for each edge on `graph.path_edges[route.path]` with a non-empty `copies[e]`, finds the sibling route of the same path with a different copy, then the LaneSplit token in `programs.copies[e]` (copy-1 tag) and LaneMerge in `programs.edges[e]` (copy-0 tag). At width 2 `copies[e]` is non-empty for every edge, so this runs, finds no split/merge SBend (`with_role` returns size) and emits nothing (INFERRED safe; its `copy != route.copy` sibling is track 1). With 4 routes per (edge,track) the sibling must be matched on (track) and copy, and the split of copy c vs merge of copy 0 matched per track.

Hub composition (block/junction_hub.cpp): `core_route_of[{edge, copy}]` (:149-153) and `core_route_of.find({stage.a, stage.b})` (:402): core copy = track, one route per (edge, track), 2 per edge. `copies` (:381) counts a hub path's block-side doubled edges (a block path with 2 routes) and refuses a second one (:390 "a hub path passes more than one doubled block edge"). A hub path through a 4-deck core edge would meet one doubled core track and maybe a doubled block edge: the "copies" per hub path becomes core copies x block copies; the check at :390 must allow a core-side doubling. Hub programs: `hub.programs.copies[he]` copies block copies only (:361-367); core segments add no tokens today (RUN tag with `tokens` offset, :405-406), so core tokens for the new SBends need appending. Hub tag `copy` = `c` (:397,414), the hub path copy. Block (junction_block.cpp:286-290, `source.edges[e].copy`) is a different `copy` (a segment list), unrelated.

## Q6. Smallest step sequence (each one commit; width 1 bytes unchanged)

1. **H1c1 storage (identity).** Replace the pair `programs.edges/copies` by an accessor, or keep both and add `programs.chains` (per edge, vector of programs); simplest: leave `edges`+`copies` for copies 0/1 and add a lookup `ProgramOf(e, chain)` and placement `chains[e]` (vector<vector<Elem>>) where `chain[e]` = chains[e][0], `chain_copy[e]` = [1]. First commit exact changes: (a) in placement.cpp turn `chain_copy` into a view `std::vector<std::vector<Elem>>& chains[e]` plus `chain_count(e)`; replace each `{&chain[e], &chain_copy[e]}` loop by `for (auto &c : chains[e])`; replace `lane_bend`'s pointer test with a chain index; (b) widen `placement.bridges` to `vector<vector<JunctionBridge>>` (render.cpp:118, legacy/junction_nodes.cpp:795 adapt); (c) `Occupant::copy` flat chain id. Identity gate: all current tags unchanged. This is largest, mechanical, about 100 edits in placement.cpp.
2. **H1c2 stack items.** Add `Kind::TrackCopy` (or reuse `Copy` + `track`) and `RowItems::track_copy[2]`; emit in the width-2 branch (topology.cpp:957-975) only when the row has a crossed edge (copy the :966-968 test). Identity: crossed width-2 rows are the only change (new gate for the bundle T).
3. **H1c3 programs.** Width-2 crossed row edge (:1226-1237): per track add LaneSplit/LaneMerge SBends and four program lists (needs storage from step 1 for programs.copies>2 per edge; else programs.copies[e] becomes vector of vectors).
4. **H1c4 placement relations.** Unify the `!bundle` doubled-leg block and the bundle row block (:2413-2535) into one per-track block with the head stagger within a track and across tracks (D118/D287); occupancy `related`; sym_routes route key (edge, track, copy).
5. **H1c5 render + signals + hub.** render copy loop, `JunctionRouteEvents` sibling match per track, hub `copies` limit (:390) and core `core_route_of` key, plus check that tokens of core edges are appended to hub programs.

Open question for the orchestrator (INFERRED): whether a hub path through a doubled core track AND a doubled block edge is meant to be allowed (hub.cpp:390 forbids it today).
