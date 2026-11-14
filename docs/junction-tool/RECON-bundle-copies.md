# RECON: can the lane+copy machinery carry a bundle?

Read-only recon at HEAD 52401a10ae. Paths are under `src/junction/` unless noted.

## Summary

1. Verdict: the copy machinery is NOT a usable base for bundles as it stands. Its bricks (copy chains, HeadsPair, the stagger, double Under tokens) are reusable.
2. Biggest blocker: a copy is a second route of the SAME lane path that splits from the lane and merges back. A bundle track is a different path with its own ends and no split or merge.
3. Doubling exists on row edges only (leg, crossed in edge, crossed out edge). Band edges are single by construction and refuse doubling; there is no width notion on a band edge.
4. The single-track T core is small: 6 nodes, 12 edges, 3 crossings. If nodes become block ports, the core keeps the 6 legs and 3 crossings.
5. Better base: generalise `copy` into a track index per edge (width w), owned by lane paths, without lane split/merge roles. Same data shape, different meaning.

## Q1. Where "doubled" is decided

- Type: `JunctionTrackBridge::doubled` (`topology/junction_topology.h:187`). "A main-line bridge (D80, D188); a branch bridge is single (D174)".
- Set in exactly three places, all in `StructureJunctionTrackGraph` (`topology/junction_topology.cpp:838`):
  - `:917` `if (is_row_edge(e) && !order.edge_crossings[e].empty()) structure.bridges.push_back({e, order.edge_crossings[e], true});`. Any crossed row edge gets a doubled bridge.
  - `:950` and `:966` push `false` for band-band crossings (joiner over leg, joiner over joiner, leg over leg).
- `is_row_edge` (`:857`) is "both ends on the same odd level". So doubled can be set on:
  - the row leg (node to node): yes.
  - a crossed out edge (node to port): yes (`:1164`, programs).
  - a crossed in edge (port to node): yes (`:1146`).
  - band edges (legs/joiners between bands): never.
- Programs side: `doubled_bridge(e)` (`:1048`) and `row_doubled[r]` (`:1067-1073`, only when the edge has nodes at both ends, i.e. the leg). `programs.copies[e]` non-empty is the "edge has a copy 1" flag (`JunctionIsDoubledEdge`, `:229`). Placement has its own `IsDoubledEdge` / `doubled_edge` (`placement/junction_placement.cpp:819`, `:993`); render has `doubled_leg` (`placement/junction_render.cpp:46`).
- Refusals elsewhere:
  - `topology.cpp:1030` "an edge between two rows is not ruled yet".
  - `:1040` "a lane other than in, one leg, out is not ruled yet" (each row needs exactly one in, one leg, one out).
  - `:1142` "a row without its side tracks is not ruled yet".
  - `:1152` "a crossed in edge whose node sits off its lane and copy items is not ruled yet".
  - `:1229` "a crossing without a bridge is not ruled yet". Docs (`topology/CLAUDE.md:28`): "a through edge off its row in a crossing is refused as not ruled".
  - `placement.cpp:350` (band edge element): `structure.bridges[*bridge_it].doubled || crossed.size() != 1` gives "a bridge over several band crossings is not ruled yet". So a doubled band bridge is refused here.
  - `:825` "a doubled leg without its lane split and merge is not ruled yet".
  - `:855-` `Over && doubled` on a row edge fails with "an Over token of a row edge without a bridge is not ruled yet" when no bridge matches. Any other token kind: "a token of a row edge other than its fan, S-bends and bridges is not ruled yet".
  - `topology.cpp:1528` `NOT_REACHED` if a band edge program has copies.

## Q2. Where the copy track exists

- It is a whole second chain per edge, not only a stretch between two S-bends. Chains are `chain[e]` (copy 0) and `chain_copy[e]` (copy 1) (`placement.cpp:239`). Each chain runs start tile to end tile.
- But the two chains share track outside the S-bends. Doc comment `placement.cpp:1207`: the copies of a doubled leg share one lane before the lane split and after the lane merge (copy -1); copy 0's run begins at the split, copy 1's run ends at the merge.
- Leg program (`topology.cpp:1138-1145`):
  - copy 0 (`programs.edges[e]`): `Over...` tokens, then `SBend LaneMerge` onto the lane row.
  - copy 1 (`programs.copies[e]`): `SBend LaneSplit` off the lane row, then `Over...` tokens.
  - So copy 0 keeps its upstream row through the bridge and bends onto the lane row after it; copy 1 bends onto the copy row before the bridge. Between split and merge there are two parallel tracks over a parallelogram (copy 1's heads one tile upstream of copy 0's, `topology/CLAUDE.md:50`).
- Out edge (`:1164-1173`): copy 0 `Over, SBend LaneMerge, Fan`; copy 1 `SBend LaneSplit, Over`.
- In edge (`:1146-1159`): copy 0 `Fan, Over, SBend LaneMerge`; copy 1 `SBend LaneSplit, Over`.
- Row items: stack item kinds `Lane`, `Prio`, `Copy`, `PrioAndCopy`, `Approach` (`junction_topology.h:167-174`); `row_items[r].copy` is the Copy or PrioAndCopy item.
- Token fields: `Step::copy` is only for `Under` (which copy of a doubled bridge is overhead, `:229`); `Step::Role::LaneSplit/LaneMerge` mark the two S-bends (`:219-220`). There is no `copy 0/copy 1` token otherwise; the copy is the separate `copies[e]` vector.

## Q3. Stages the copy flows through

- Order: `OrderJunctionTrackGraph` has no copy notion. Copy items appear only in the structure's `stack`.
- Structure: `StructureJunctionTrackGraph` (`topology.cpp:838`): stack with `Copy`/`PrioAndCopy` items per row (side_item, ~`:900`), doubled bridges (`:917`). The T's stack: row 1 is `PrioAndCopy, Lane, Approach` (test `tests/junction/topology/junction_topology.cpp:489`).
- Programs: `ProgramJunctionTrackGraph` (`:1000-1235`): `programs.copies`, `row_doubled`. Arrangement (`:1462-1475`): per doubled bridge, two adjacent `Under` tokens on each crosser (one per copy) and the pieces that may pass under.
- Pipeline: `CutJunctionTrackBridges` (`junction_pipeline.cpp:111`) cuts doubled bridges into series bridges (cap `JUNCTION_BRIDGE_MAX_CROSSERS` = 2) by composition search; only `doubled` bridges are cut.
- Placement (`placement/junction_placement.cpp`, `PlaceTrackGraph` `:87`):
  - chain builder for row edges (`:806-870`): per (edge, copy) chains, `Elem::K::HeadsPair` for a doubled Over.
  - bridge head lookups `heads_of` (`:3326`), crosser tile hugging (`:437`, `:2008`, D217), bridge lines (`:2028-2043`).
  - doubled-leg relations (`:2199`, `:3382`): lane pair joins the two chains, copy 1 heads = copy 0 heads one tile upstream (D118), S-bend span per copy.
  - spans: `placement.copies[e]` (`junction_placement.h:278`, `bridges` is `array<JunctionBridge,2>` per bridge).
  - Positions/MILP: `junction_positions.cpp` and `junction_milp.cpp` have no copy mention. Copies enter only through elements and relations built in `junction_placement.cpp`. `junction_joint.cpp:424-503` keys lengths by `(edge, copy)` and adds a chain per copy for any path crossing a doubled edge (`chain_len`, `:461-471`).
  - Rule floor placement text: `placement/CLAUDE.md:7`.
- Render: `RenderJunctionTrackPlacement` (`junction_render.cpp:33`): per lane path, `c` loops 0..1 when any edge of the path is doubled (`:62-63`). One route per (path, copy). Copy 1 reads `programs.copies[e]` and `placement.copies[e]` (`:86-88`), heads `placement.bridges[b][c]` (`:99`). Tags carry `copy` (`:204`).
- Levels: `junction_levels.cpp:49, :287, :1539` carry `copy` from route tags (the level solve treats copies as separate routes).
- Signals: `SignalJunctionTrackRoutes` / `JunctionRouteEvents` (`signals/junction_signals.cpp:35`, `:87`): the lane split and merge are switches both copies pass, located by the S-bend role (`:47-60`); sibling route found as same `path`, other `copy`.
- Checker: `CheckJunctionRules` (`junction_rules.cpp:619-628`) "ML bridge is not doubled" for main-line bridges; `:165` "the routes of one doubled lane path".

Key point: in render, levels and signals a copy is another ROUTE of the same lane path. That is the choice-split semantics (PSG 227's doubled-bridge choice).

## Q4. Band edges

- No width notion. Band edges are crossed by single bridges only: one per crossing, in series (`topology/CLAUDE.md:35`: "a band edge carries one single bridge per crossing"). Placement refuses a doubled or multi-crossing bridge on a band edge (`placement.cpp:350`).
- Band edge programs (`topology.cpp:1180-1235`): optional `Fan`, then start (`Corner` for a joiner arrival, `SBend Diverge`), crossings (`Over`/`Tunnel`, `Under` x2 for a doubled crosser, or `SBend Cross`, `Corner Cross`, `Under`/`Above`, `Corner Cross` back), arrival (`Converge` or `Corner`). `ends` come from the program (`:1526`, `NOT_REACHED` if copies).
- To carry a band edge on two parallel tracks:
  - Program tokens: a track index on every token (or a second program vector) in `ProgramJunctionTrackGraph`; Corner/SBend get a second parallel line (an offset row or column).
  - Placement element builder for band edges (`placement.cpp:~300-420`, the `Elem` loop around `:340-352`) must emit per-track chains; its `Over` branch needs a HeadsPair pair like the row chain builder (`:854`), not the single `Heads`.
  - Corner of a parallel pair: the outer track makes a larger radius. That needs a rule (turn spacing) that nothing has today: curve-gap and `JunctionUnrelatedTileGap` (`placement/CLAUDE.md:8-9`) assume unrelated tracks; a bundle track pair is related and adjacent.
  - Arrangement (`junction_topology.h:338`, `JunctionArrangementUnder`): Under pieces per crossing must be per track and per copy: crossing a 2-wide bundle under a 2-wide bridge is 4 pass-unders.
  - `CutJunctionTrackBridges`: handle band bridges (they are `doubled == false` today and skipped).

## Q5. Nodes of the single-track T (L_R x L_R)

From `tests/junction/topology/junction_topology.cpp:182-210`:

- Nodes (6): splits N0 (slot 0, 1), N1 (2, 4), N2 (4, 3); merges N3 (1, 3), N4 (3, 4), N5 (5, 1).
- Edges (12): 3 In (port to split, paths {0,1}, {4,5}, {2,3}); 6 Legs (N0 to N5, N0 to N4, N2 to N3, N2 to N4, N1 to N3, N1 to N5; one path each); 3 Out (merge to port, two paths each).
- Crossings: {4,5}, {4,8}, {5,8}.
- Order: node levels {1,4,3,3,4,1}; row 0 = N0, N5; row 1 = N3, E4, E8, N2; band 2 holds the three events.
- Structure: bridges {edge 5 over {8,4}, doubled}, {edge 8 over {4}, single}; stack row 0 `Prio,Lane,Approach`, row 1 `PrioAndCopy,Lane,Approach`; programs are 12 edges, copies[5] = `{SBend LaneSplit, Over 8, Over 4}`.
- Edge 5 is the one row edge crossed (the row leg on row 1). Edges 4 and 8 are band legs; edge 4 passes under 5's doubled bridge and 8's single bridge.
- With every node replaced by a block port, In and Out edges (the six end edges) become block ports and vanish from the core. What remains: the 6 legs, their ends at the ports, and the 3 crossings. The row/band structure collapses: no joiner approach, no prio, no lane split. The core then has: 1 row leg (5), 2 band legs crossing (4, 8), 3 other legs not crossing, and the Fans at the ports (the legs end at port slots, `Fan` S-bends or Corners).
- Unknown: how the graph builder would express a leg that starts at a block port instead of a node (`Edge::from` has `node` flag and `index`; a third endpoint kind is not in the code).

## Q6. Tunnels

- Field: `JunctionTrackBridge::tunnel` (`junction_topology.h:188`), comment "only on a single (not doubled) separation".
- Enforcement is by omission, not by a refusal:
  - Nothing in the pipeline sets `tunnel` (only tests set it, `tests/junction/topology/junction_topology.cpp:557`, `:589`).
  - Row edge programs push `Over` for every crossing regardless of `tunnel` (`topology.cpp:1139-1173`); only band edge programs use `tunnel_over` (`:1216`, `:1225`). A tunnel on a doubled row bridge is silently a bridge in the programs.
  - Placement band element path (`placement.cpp:348-350`) accepts a `Tunnel` token only with a single, non-doubled bridge of one crossing.
  - Placement tunnel length check (`:3463-3466`) runs inside the per-copy loop `for copy < (bridge.doubled ? 2 : 1)` (`:3455`), so the length cap is copy-aware, but nothing builds a doubled tunnel's chains.
  - Levels read `tunnel` from the pieces (`junction_levels.cpp:1544`).
- A doubled tunnel (two parallel tunnels) would need:
  - Row-edge programs: emit `Tunnel` (and `Above` on the crosser) when `tunnel_over` holds, with a copy 1 `Tunnel`.
  - Placement row chain builder: a `Tunnel` token on a doubled edge produces `HeadsPair` (portals). Lines `:2037-2038` already skip the deck clearance for a tunnel.
  - Render and ASCII: two portal pairs, one per copy (`placement.bridges[b][copy]`; render uses `tunnel` per route).
  - Parallel tunnels share tiles of the surface between portals: the "unrelated tracks never share a tile" rules apply above, not under; the bundle's two tunnels run on adjacent rows, and surface tracks crossing above both is fine.
  - Rules/checker: a tunnel is one wormhole with no signals inside; the checker side (`junction_rules.cpp` bridges rule) needs to accept a doubled tunnel.

## Q7. Work estimate

Order of steps; each is one commit with its own test. M = mechanical, D = design-bearing.

1. **Pin the T's current doubled facts as tests (M).** Golden test of copies[5], chains, routes for the T; no code change. Guards the later generalisation.
2. **Name the semantics: add `Edge::width` / track index to the graph's data model, copy renamed or aliased to track (D).** Decide: is a track a path (own `paths` entry) or a copy of a path? Bundle = two paths on every core edge. Nothing else changes; width 1 everywhere is the current behaviour.
3. **Core graph with block ports (D).** Build the T core: 6 legs, no In/Out edges and no nodes; a leg endpoint is a port at the core edge (Q5, unknown how). Fixture + graph test (6 legs, 3 crossings).
4. **Order and structure for the port-only core (M, after step 3).** Rows and bands; bridges: leg 5 over {8,4}, leg 8 over {4}; every one a bundle bridge. Test the T core's order and structure.
5. **Programs for width 2 on row legs without lane split/merge (D).** Leg track 1 runs a parallel row from port to port; bridge tokens on both tracks as `Over` pairs; no `LaneSplit`/`LaneMerge` roles. Reuse `copies[e]` shape, but the two tracks are two paths. Test the program golden.
6. **Programs for width 2 on band legs (D).** Corners and S-bends of a parallel pair (outer and inner), Under x4 on crossers. Test golden.
7. **Placement chains for parallel tracks (D).** Chain builder: per (edge, track) chains for row edges (reuse `chain_copy` as track chains) and band edges (new, replaces the `doubled` refusal at `placement.cpp:350`); relation: adjacent tracks one row/column apart; heads of track 1 one tile upstream (reuse D118 stagger). Test: the T core places, size recorded.
8. **Render and routes per path (M).** One route per path as today (a path owns its track), no copy loop; the `doubled_leg` copy loop in `junction_render.cpp:62` becomes a path-owned track lookup. Test: ASCII and layout of the T core.
9. **Signals and checker (D).** Signal each track as its own lane (no lane split/merge switches, so `junction_signals.cpp:47-60` is skipped); the checker accepts two parallel main-line bridges per leg with no choice (`junction_rules.cpp:619`). Test: `CheckJunctionRules` clean.
10. **Bridge cutting for series bundles (M).** `CutJunctionTrackBridges` over band and row bundle bridges (cap 2 crossers). Test on a bundle T with the cap lowered.
11. **Doubled tunnels (D).** Row/band `Tunnel` tokens x2, portals, length cap. Test: the T core with tunnels, checker clean.
12. **End-to-end "every edge of the T is a bundle of 2" (M).** Place, render, signal, check; record in STATUS.

## Why the copy machinery is the wrong base (3 lines)

- A copy is an alternative route of one path (split, merge, a choice); a bundle track is its own path with no switch, so the signals, levels and routes treat them wrongly.
- The data shape (per-edge parallel chains, HeadsPair, stagger, double Under) is what a bundle needs; keep it, drop the lane split/merge roles and the row-only restriction.
- Better base: the generalised track index of step 2, owned by lane paths, applied to row and band edges alike.
