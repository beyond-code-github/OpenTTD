# Prio freedom: bridge, bend, diagonal, offset (research, 2026-11-18; read-only, no builds)
F = fact (cited), I = inference. Corpus = OneDrive scratchpad (coop-site/site/Priorities.html, coop/files/*.png, junctionary-mirror/pages/Priority Merge.mediawiki).

## Game mechanics (src, F)
- Signals only on plain rail tiles: rail_cmd.cpp:1077 (`IsPlainRailTile`). No tunnel/bridge signal simulation in this build (grep WithSignalSimulation: none). So no signal on a bridge head or middle.
- signal.cpp:382-395: block exploration enters a bridge head, jumps to the other head ("skip to exit tile"); the whole span is one block member, occupied if a train is on either head (HasVehicleOnTile) - sigsim.py:3 models the same. No block length limit in signal.cpp. A bridge inside a prio's chain is therefore a plain-less stretch of >= 2 tiles + span between the signals either side.
- Drag fill (D97) is only the placement rule (CmdSignalTrackHelper); it forces nothing on the game side. Bridge heads are "unsuitable" spots, so fill falls back to the last suitable tile before the head (I).

## (a) Over a bridge
- Game: allowed (F, above). The prio's pre-signal chain cannot sit on the span; it can only watch the bridge as a block.
- Coop: Priorities.html headings 4, 5, 8. "Standard prio over a bridge" (parallel track, joins either side of the bridge, needs a third bridge; Standard_prio_bridge.PNG: three parallel bridges, joins before and after): unusable for TL < 4 because the gap between the connections becomes longer than the train, so a train on the span is on no joined block ("the prio won't trigger"). "Presignal prio over a bridge" (Presig_bridge_prio.png) needs no third bridge and suits low TL. "PBS prio over bridge" needs no parallel track (path signals; not ours, D74 is pre-signal only). RULES-coop R31, R40, R51: entry check cannot stop trains on bridges; evil mode; best remedy is not bridging the prioritised line.
- Constraint (F, D107/G9b): every ML (tile, track) in coverage, plus the tile before the first upstream signal, must settle the Entry red for a one-tile train. A bridge block is one unit, so it needs one join on a block it belongs to, but the span has no tile that can carry h1 (head tiles hold the bridge only; RECON-prio-footprint.md:21 "no bridge head in it", currently a T3 condition).
- Solver rule (I): a bridge may lie in coverage only if the parallel row (or a second ML bridge) is joined on a plain ML tile in the same block as the span: i.e. the plain tile(s) just before and after the bridge each carry a join, and the bridge span (head to head) is shorter than the shortest train (TL tiles, same gap rule as R25 in RULES-coop.md ~l.124-130). Otherwise forbid. Needs a second prio-row bridge (the prio row crosses the same obstacle); count its cost.

## (b) Bending
- Game: nothing stops a prio track bending; joins are two 45-degree pieces on any suitable tile (F, D75).
- Coop: Used_tunnel_prio.png and Presig_bridge_prio.png show prio rows with bends/S-bends around the join pair (F, viewed). Priorities.html "Limited space" shows a same-side prio.
- Ruled: D111 (F): prio may curve or S-bend parallel to the ML, joins on suitable pieces; G9 shape clauses follow the ML path. NOT implemented: PlacePrio/CheckPrio are straight-only (RECON-prio-footprint.md:20).
- Constraints (F): D108/G9c no split of ML lanes inside coverage + one segment; T3 rule "no curve within 2p units before a prio merge" (RECON-prio-rule.md:18, DESIGN-N3) and RECON-prio-element.md:43 "no ML split, curve or lane merge on offsets 1..p" - these contradict D111 and need a ruling; coverage counts units (axis 2, half 1), so a bend shortens tile count, not coverage (F, RECON-prio-element.md:4).
- Solver rule (I): coverage = walk the ML path by units; a join is allowed on any piece where a free parallel row piece exists (axis piece, or a half pair on a bend as the row parallels it); the joiner side stays the opposite side of the bend (inner/outer row lengths differ: the outer row is longer, so segment length must be checked against TL on the row, not the ML).

## (c) Diagonal
- Meaning (I, ask Pete): "diagonal" in the code = axis X/Y pieces (they show diagonal on screen); the 45-degree half pieces are "half". GeometryOfPrio needs an axis merge piece (RECON-prio-footprint.md:6). If Pete means ML runs of half pieces (true 45-degree tracks), they are the same as bends: unit weights 1 each, signals on half pieces are legal (CmdSignalTrackHelper weights 128, D97), but a half-piece tile carries one track with the other half unused, so the row cannot share the tile.
- Coop: no text rule; no image in the viewed set shows a prio on a long 45-degree run (not found).
- Solver rule (I): treat as (b); require each join on a piece that has a free parallel piece in the neighbouring tile.

## (d) Placed further along its line from its join
- Facts: the prio row is a parallel track whose first join is on the merge tile itself (D79, D81); D59 (superseded) had a tail. Coverage runs upstream of the merge and its blocks need one join each (RECON-prio-element.md rule 1). Priority Merge.mediawiki l.10: basic two-signal prio reaches only two signal lengths; l.17: too much space in the middle lets a train sit unseen.
- Coop: Used_tunnel_prio.png / Presig_bridge_prio.png show the prio loop starting some tiles from the join that closes the loop (F, viewed); no stated rule on the offset.
- Constraint (I): the signal at the merge must stay within the chain: any gap of plain-less tiles between merge and the first join breaks D107 (the gap segment holds an unseen train). So the offset is free only if every ML tile between merge and the first join is in a block that is joined (or the gap is shorter than the shortest train and the held-train tile is covered).
- Solver rule (I): leave row start s0 as a variable; require joins at every even offset from 0 as today, i.e. offset only means the row may leave the ML after the last needed join and rejoin first at the far end. "Further along" for the downstream end is not available: join on the merge tile is required (D79).

## Open for Pete
1. Which "diagonal" (axis vs 45-degree runs)? 2. Overrule the T3 "no curve within 2p units" rule (DESIGN-N3) in favour of D111? 3. Bridge-in-coverage: accept the span-shorter-than-TL rule or forbid?
