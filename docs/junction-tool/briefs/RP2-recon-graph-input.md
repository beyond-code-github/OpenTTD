# RP2 (recon): the node template's input from the graph, not from a layout (D201, D213)

Read-only recon. Do not commit code. You may add a temporary hidden diagnostic TEST_CASE (`[.][diag]`) in src/tests/junction_topology.cpp; remove it before you finish. Keep your context small: send command output to files and report only what failed. Never use git stash. Mark anything you did not read in code as INFERRED.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git checkout -B wt-rp2 <HEAD>` (given at launch).
- D213 (DECISIONS.md, read it, and D201): projection works on the graph; a layout is only output. Today the node template's input (JunctionNodeInput: paths, through, exit, used, events, slots, bridges; junction_nodes.h) is traced from a layout (TraceJunctionNodeInput(layout, spec, topology), junction_nodes.cpp). The G1 pipeline (see SignalledOf in src/tests/junction_topology.cpp) has everything as graph data: graph (nodes, edges), structure, programs (tokens per edge), placement (per node tile, per edge per token span, bridges, ports), routes (RenderJunctionTrackPlacement's routes out-param: per lane path and copy, its pieces), signal tokens (SignalJunctionTrackRoutes).
- Wanted later: build JunctionNodeInput from those, with every path piece tagged by its graph edge and token (or "run between tokens"), so solve failures name graph elements and rules attach to them (D212 on the port edge's Fan token).

## Read first (once each, offset/limit)
- DECISIONS.md D201, D212, D213; DESIGN-G1.md §0, §0b, §11.8-§11.10, §12.
- src/junction_nodes.h JunctionNodeInput; junction_nodes.cpp TraceJunctionNodeInput.
- src/junction_rules.h/.cpp: JunctionPath, MakeJunctionTopologyCheckContext (how paths are traced and ordered, how from/to roles are set), UsedTracks, SwitchEventsOf, IsLegalSignalPiece, IsSignalledPiece (what each reads from the layout).
- src/junction_topology.h :195-285; junction_topology.cpp RenderJunctionTrackPlacement (its route loop: where each piece is appended and which edge/token it belongs to).

## Questions (answer each with file:line)
1. **Path order and identity.** In what order does the checker context list paths (rule, as a function of what)? Can that order be computed from the topology/lane paths/copies alone? How are JunctionPath.from/to (roles), from_slot, to_slot, movement set, and is there a reusable slot-to-role function?
2. **Pieces.** Diag: for the T and LL_RR seeds, compare each route's pieces (tile, trackdir, on_bridge) with the checker path's pieces of the same lane path and copy. Equal? List any difference (first 5 per path).
3. **used.** What UsedTracks includes (bridges? signals? prio tracks?). Can it be the union of the routes' piece tracks plus the prio element tracks? Diag: compare.
4. **events.** What SwitchEventsOf reads. Can each event be read from the graph (a route's piece on placement.nodes[n] of a split or merge node n it passes)? Diag: compare per path.
5. **slots.** IsLegalSignalPiece/IsSignalledPiece read the layout's signals and track. Can slots come from the routes plus the signal tokens (a token's piece, or a legal piece by the anchor rule ok(i) in SignalJunctionTrackRoutes)? Diag: compare per path.
6. **bridges.** layout.bridges vs placement.bridges (copies, order, z). Equal as sets? Order?
7. **Tags.** In RenderJunctionTrackPlacement's route loop, can each appended piece be tagged with (edge, token index or none, copy)? Name the exact lines where the edge and token are known.
8. **The oracle (DESIGN-G1 §0b).** If input is built from the graph in the checker's path order, would DiffJunctionNodeTemplates against the traced template be empty for the T? Name every field that could differ.

## Output
docs/junction-tool/RECON-graph-input.md, at most ~100 lines, one section per question, then "Recipe": per JunctionNodeInput field, its graph source in one line. Leave it untracked; commit nothing.

## Report
References read; the recipe; any field that cannot come from the graph (and why); `git status --porcelain` and `git log -1`. When you have reported, stop.

## Additions (DESIGN-G1 §13.8 item 10, FABLE-projection)
9. **The token pass's own layout reads:** SignalJunctionTrackRoutes reads SwitchEventsOf over layout tiles (junction_topology.cpp ~:1861, :1952, :2017, :2073), finds the joiner by tile coincidence (~:1863-1876), and calls IsLegalSignalPiece/IsSignalledPiece on the layout (~:1969, :2046). For each: what graph data replaces it (nodes, routes, tokens)?
10. **The builder's tile-coincidence reads:** prio detection and joiner_row (junction_nodes.cpp ~:355-365), anything else in NodeBuilder that compares tiles across paths. Graph replacement for each.
11. **LL_RR** in Q2 (pieces) and Q8 (oracle): include it; NP2 made the LL_RR seed traceable, so the oracle is exact equality (list order ignored) for both topologies.
12. **Timing (Release, median of 5):** one LL_RR state: placement (CutJunctionTrackBridges), routes (render), template build, solve on flat facts. Same for the T.
13. **Doubling decorations:** the lane split/merge of doubled edges are not graph nodes: where are their tiles in placement/programs (§11.10: copy 1's SBend first piece, copy 0's SBend last)?
14. **Anchor:** what NodeAnchorShift and tpl.anchor are used for (offsets in GenerateJunctionNear / the search), and whether the placement's port slots give the same translation reference.
