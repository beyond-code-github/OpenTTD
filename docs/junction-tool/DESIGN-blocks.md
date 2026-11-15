# DESIGN-blocks: block templates (B2, the PSG 227 join block first)

Orchestrator design, 2026-11-15. It implements DESIGN-modules §4.1 and §6 B1-B2 on the REVIEW-width-overprescribed Q4 interface: per-track ports, floors between them, pieces as no-touch occupants, stretch by floors. D193 (valid by construction), D282/D283/D287 (prio on the fixed track; our prio element and parallelograms), D296 (no invented geometry), D297 (seed under 5 s).

## 1. Idea
A block is a **block program**: data, not code per shape.
- **Points.** Column variables along the block's heading. Columns are solved as the least solution of difference constraints (`JunctionPositions`), so the solve is cheap.
- **Lines.** Each track runs on a line across the heading. Lines are template data: integers, where 0 is the outermost line on the left of travel.
- **Edges.** Each graph edge is one track from a start (port or node) to an end (node or port). It is a list of segments:
  - `Run(to point)`: straight along the current line to the point's column;
  - `SBend(lines)`: a sideways step of a signed number of lines (+ is right of travel). It starts at the current column, and its width is measured from its raster;
  - `Bridge(to point)`: the near head is at the current column and the far head at the point.
  - An edge ending at a node ties the node's point exactly to the column where its last segment ends.
- **Paths.** Each lane path is a list of edges. The route of a path is its edges' moves joined (D295 one route per track).
- **Rule families** turn the program into floors. They apply to every program, never to one site:
  - port stub;
  - signal room around splits, merges and heads;
  - the bridge-over-diagonal hug;
  - queue bays;
  - no touching per line;
  - curve gaps per path;
  - prio room.

  A floor exists only because a rule asks for it. If the checker run finds a violation, the template is missing a rule (D193), and we never add padding to make it pass.
- Every quantity a floor needs (an S-bend's width, which columns it covers on each line, its path units, its bend positions) is **measured from the raster of the segment's moves**. Nothing is typed in as a per-shape constant.

The same machinery later carries the keep and balanced-no-prio joins, the exit block (B3) and width 3. Each is a new program table, not new code.

## 2. The PSG 227 join block (two bundles in, two lanes out)
The block heads east. Lines run top to bottom (0 is the north side):

| line | west (in) | east (out) |
|---|---|---|
| 0 | | prio row of L0 |
| 1 | fA (bundle A, fixed) | L0 |
| 2 | cA (bundle A, choice) | |
| 3, 4 | free (cA1 and cB0 run here) | |
| 5 | cB (bundle B, choice) | |
| 6 | fB (bundle B, fixed) | L1 |
| 7 | | prio row of L1 |

Graph (§4.1): splits sA (cA) and sB (cB); merges mC0 = cA0 + cB0, mC1 = cA1 + cB1, mL0 = fA + c0 (prio on fA), mL1 = fB + c1 (prio on fB); one crossing, cA1 over cB0. Edges, with their segments:
- fA: Run(mL0). It is straight on line 1. L0: Run(E).
- cA In: Run(sA). cA0: Run(mC0) on line 2.
- cA1: SBend(+1), Run(hN), Bridge(hF), Run(d1), SBend(+2) ends at mC1. Line 3 carries the bridge; the descent runs 3 to 5.
- cB In: Run(sB). cB1: Run(mC1) on line 5.
- cB0: SBend(-1), Run(b0), SBend(-2) ends at mC0. It runs 5 to 4, then along 4, then 4 to 2. Its diagonal crosses line 3 under cA1's span.
- c0 (mC0 to mL0): Run(e0), SBend(-1) ends at mL0. c1 (mC1 to mL1): Run(e1), SBend(+1) ends at mL1.
- fB, L1: the mirror of fA and L0.

Paths and reach (D283):
- fA→L0: fA, L0.
- cA→L0: cA In, cA0, c0, L0.
- cA→L1: cA In, cA1, c1, L1.
- cB→L0: cB In, cB0, c0, L0.
- cB→L1: cB In, cB1, c1, L1.
- fB→L1: fB, L1.

So each fixed track reaches its own lane, and each choice track reaches both lanes.

The bundle gap at the west ports is 2 lines (lines 3 and 4). That is a floor the core must meet, by Q4 point 2. The out lanes sit on lines 1 and 6. Bringing them together for the arm is composition's fan (H1), not part of the block.

### 2.1 After B2b (D298, D299)
- The crossing stays a single bridge. The engineer ruled that choice legs are not doubled, because they are already alternatives to each other (D299). A doubled table was drafted here and then withdrawn.
- The topology lists the prio movements fA→L0 and fB→L1 (D298). With that list, the checker asks for a prio only at mL0 and mL1. It also lets the bridges of unlisted (choice) movements stay single.
- The block's box keeps one empty line beyond each prio row, because the checker allows only ports on the box edge.

### 2.2 The exit block (B3): one input arm, two bundles out
Each input lane splits by destination, and the branches regroup into one bundle per destination (§4.1 of DESIGN-modules). The crossing is l0b over l1a. It carries main-line traffic that has no alternative track, so it is a doubled bridge (D80, D188). mfb's E exit block does the same ("D pair", RECON-mfb-core 9). D299 does not apply, because l0b and l1b are not alternatives to each other.

The block heads east. Lines (0 is north) after normalising:

| line | west (in) | east (out) |
|---|---|---|
| 0 | l0 | l0a (bundle A) |
| 1 | l1 | l1a (bundle A) |
| 2 | | copy 0 deck of l0b; free at the out edge |
| 3 | | copy 1 deck of l0b, then l0b (bundle B) |
| 4 | l1 after its sideways step | l1b (bundle B) |

- **l1 In**: Run(p1), SBend(+3) (1 to 4), Run(s1).
- **l0 In**: Run(s0).
- **l0a**: Run(E).
- **l1b**: Run(E) on line 4.
- **l1a**: SBend(-3) from s1 (4 to 1), then Run(E). Its diagonal passes under both decks.
- **l0b (doubled)**:
  - copy 0: SBend(+2) from s0 (0 to 2), Run(ls), Run(hN0), Bridge(hF0, crosses l1a), Run(q0), SBend(+1, LaneMerge), Run(E);
  - copy 1: SBend(+2), Run(ls), SBend(+1, LaneSplit) (2 to 3), Run(hN1), Bridge(hF1, crosses l1a), Run(E).
  - The copies are equal in length, because each has the same bends (D80 line sync).
- **Paths**: l0→l0a, l0→l0b (two routes, copy 0 and copy 1), l1→l1a, l1→l1b. There are no merges other than the lane merge, so the block has no prio list, and the doubled-bridge rule applies.
- **Doubled-edge machinery**:
  - `JunctionBlockEdge::copy` holds copy 1's segments, and an S-bend may carry a role (LaneSplit or LaneMerge).
  - In programs, a doubled edge gets one SBend token per copy: copies[e] has role LaneSplit and edges[e] has role LaneMerge. Their bend pieces are tagged with token 0 and their copy number, so `JunctionRouteEvents` finds the lane split and lane merge exactly as it does for render's doubled legs.
  - The two copies of one edge are related (R7), and R4 applies to each deck against the crossed segment on that deck's own line. The parallelogram comes from the hug.
- **Topology**: the W groups are l0 and l1. The E groups in slot order are l1b, l0b, l1a, l0a (the E side runs south to north). All are single-lane and main. The movements are the four paths.
- **Bundle ports**: A is lines 0-1 and B is lines 3-4, with one line between them. Composition (H1) reads them as per-track ports.

## 3. Rule families (the floors)
Units are the rasteriser's piece units: an axis tile is 2 and a diagonal half piece is 1, as the checker counts them. TL is `spec.tl` and P is `spec.prio_tiles` rounded up to even.

| # | Family | Floor | Rule |
|---|---|---|---|
| R1 | Port stub | Every track's first node or bend is at least 2 columns inside the in edge. Every out track runs at least 2 columns from its last node or bend to the out edge E | D206, D115a, `JunctionRunFloor` Port/Node 2 |
| R2 | Split to near head | At least one plain axis tile on the head's line between a split's bend and the near head | signals: "Between a split and the next bridge head there is a signal-legal piece" |
| R3 | Far head to merge | At least one plain axis tile between the far head and the next bend | signals: "likewise a merge after a far head" |
| R4 | Bridge over a diagonal | The crossed segment's pieces on the bridge's line lie strictly between the heads, and none touches a head tile (`JunctionUnrelatedTileGap` of the head's axis piece and the nearest diagonal piece) | placement: "a single bridge … crosses the edge's S-bend at a column its diagonal allows"; D217 |
| R5 | Queue bay | On every non-through path into a merge, the units from the piece after its upstream split to the last signal-legal piece before the merge are at least 2 TL. A piece is not signal-legal when another track stands on its tile | checker WaitingRoom (D83, D88) |
| R6 | Merge signal room | At least one plain axis tile between a merge tile and the next bend on the merged track | signals AfterMerge/BeforeMerge |
| R7 | No touching per line | On each line, unrelated occupants in their template order (west to east) are at least `JunctionUnrelatedTileGap` apart. Related means the two tracks of one node, at that node | placement no-touch family, D217 |
| R8 | Curve gaps | `JunctionCurveGaps` over each path's bends. A gap between two bends of one fixed shape must already hold (assert); any other gap is a floor between points through the units affine in columns | D93, `JunctionCurveGaps` |
| R9 | Prio room | The fixed track has no node and no bend from mL - P - 1 to mL. The prio row line (0 or 7) holds nothing else. mL - P is at least 2 (outside the in stub) | signals prio rules; checker "no split of a main-line lane inside a prio's coverage" |

R5 does not apply to the through paths fA→L0 and fB→L1. The template's topology makes them the through movements (§4).

The least solution is the block at its tightest. The stretch variables (split spacing, prio length, bridge length) are the same points, and floors only bound them below, so composition can lengthen any of them without breaking a rule.

## 4. Outputs (the seam, RECON-template-seam (a))
`JunctionBlock`:
- `topology`: single-lane groups. W: fA, cA, cB, fB, all main, on lines 0, 2, 3, 1. E: L1, L0 in slot order (south to north on the E side), on lines 1 and 0, all main. Movements: the six paths. fA→L0 and fB→L1 are same-line through movements, and the other four are turning movements.
- `paths`, `graph` (nodes, edges, path_edges; crossings = {cA1, cB0}), `programs` (`edges` and `copies` sized to the edges and empty, `nodes` and `ends` empty): enough for the signal pass (RECON-template-seam §3).
- `routes`: one per path, from the west port tile to the east port tile. Tags per piece: the piece's edge, token `RUN`, run 0, copy 0.
- `layout`: the routes rasterised and normalised.
- `prio`: two `JunctionPrioSpan`s at mL0 and mL1, built with the placement's step-bits pattern (its prio side is away from the joiner: north of fA, south of fB).
- `in_ports`, `out_ports`: per track, the tile, the trackdir and the order across.
- `points`: the solved columns, by name, for tests.

## 5. Steps
- **B2a. Geometry.** The block program types, the generic generator (measure, floors R1-R4 and R6-R8, solve, emit moves) and the 227 table. The routes, layout and ports as output.
  - Tests: connectivity through the checker's `CheckConnectivity` with the block topology (exactly the six movements); per-family invariants on the solved points; an ASCII dump to the scratchpad.
  - R5 and R9 floors are in, because they set lengths. Signals are not run yet.
- **B2b. Signals and the oracle.** The graph, tags, topology and prio spans. Run `SignalJunctionTrackRoutes`, then `CheckJunctionRules` on the block.
  - Acceptance: zero violations.
  - Each violation is reported as a missing rule family, and is not patched.
- **B3. Exit block.** A new table: 2 splits and 1 crossing, the mirror image.
- **H1.** Composition with the bundle T.

## 6. Open (orchestrator, decide at B2b's result)
- Whether a diagonal piece under a span is signal-legal. R5 counts only to the last legal piece, so the floor holds either way. B2b will show which.
- Whether `SignalJunctionTrackRoutes` puts a prio at mC0 and mC1 (choice-leg merges, D282: no prio). The template passes only the two spans. If the pass insists on drawing more, that is a signals change for B2b.

## 7. H1: composing the hub (exit blocks + bundle T core + join blocks)
Orchestrator design, 2026-11-15, at f0c6647fe3. A block is a subgraph with ports (the engineer, same day: "are blocks essentially just subgraphs?"). Its own floors size it, and the core sees it as a node with per-track ports. In H1 a block is rigid at its least solution: its port ties are exact because the template is rigid there (REVIEW-width-overprescribed Q4 point 2). Letting a block stretch inside the core's solve (its points join the core's system) is the step after H1.

### 7.1 What replaces the 4a stub (DESIGN-width R9-R13)
- **Programs declare their bundles.** `JunctionBlockProgram::in_bundles` and `out_bundles` hold port indices, each bundle in ascending line order. Bundle 0 lies on the trunk lines (the trunk of an exit block, the straight bundle and the out lanes of a join block); bundle 1 is the diverging bundle. Exit: in {{l0, l1}}, out {{l0a, l1a}, {l0b, l1b}}. Join 227: in {{fA, cA}, {cB, fB}}, out {{L0, L1}}.
- **The join block's lanes leave side by side.** L1 S-bends from line 6 to line 2 after its merge: Run(f1), SBend(-4), Run(E). So L0 and L1 are on lines 1 and 2, the trunk lines of bundle A. This replaces "composition's fan" in §2: the block owns its interior, and the core's out edge is then an ordinary bundle. R6 and R7 already give the room after mL1 and the gap to c0 on line 2.
- **Orientation.** A block is built once, heading east (x along, y the line, right of travel). In the hub it heads h = the node's trunk heading (`programs.nodes[v].heading`), mirrored when `JunctionBlockSide < 0` (the diverging bundle is on the left). The frame vector (a, c) lands at a V(h) + (mirrored ? -c : c) R(h). That is `OrientJunctionVector` with rotation (1 - h) mod 4 unmirrored, or mirrored with rotation (3 - h) mod 4. Pieces turn with the same transforms as `ApplyJunctionOrientation`.
- **Anchor.** A block's anchor is the in port of track 0 of in bundle 0 (track 0 is the leftmost of travel after orientation). This is the old stub's anchor (the upstream end on the trunk's track 0), so `node_pt` keeps its row item.
- **Attach.** Track k of edge e at block v uses port k of its bundle. For a split: trunk → in bundle 0, straight → out 0, diverging → out 1. For a merge: straight → in 0, diverging → in 1, trunk → out 0. The attach tile is the port tile minus V(h) for an in port and plus V(h) for an out port (outside the block, as before). The ties are exact.
- **Block end** (`block_end_pt`): the anchor plus E V(h), where E is the out-port column.
- **Box.** Every block tile lies inside the box: anchor plus the block's least offset is at least 0 on both axes, `xe_pt` and `se_pt` are at least the anchor plus its largest offset. A row's join block puts its prio row one line outside the lanes; the row's port fans make that room.
- **Row block gap** (the band-gap rule for row blocks; found at H1a's stop): a row block lies between its neighbour rows. For block v on row r with least and largest row offsets miny and maxy: when r >= 1, its anchor is at least 1 - miny below row r - 1's last item; when r < rows - 1, row r + 1's first item is at least maxy + 1 below its anchor. The box covers the outer sides. In a bundle graph the stack origin is a floor (item 0 at least row 0), not exact, so the least solution moves the first lane down when a block needs room north of it (the join block's L0 prio row). Width 1 keeps the exact origin.
- **Clearance at flat side 0** (found at H1a's second stop: edge 8's north head of its bridge over edge 4 lies on edge 5's copy-1 deck row inside its span, at the flat Y solve): a head lying on a crossing bridge's row inside its span keeps the side of its own bridge's other head (it moves into its own span) by one, in Y as in X, instead of demanding both sides. Width 1 never reaches this branch in a passing placement (it would have failed), so it does not move.
- **S port fans for bundles** (found at H1a's third stop: the S in and S out bundles' blocks overlap at their port columns, which the notation pins 3 apart): in a bundle graph each S group is one bundle, so the existing S fan rule (west half of a group's lanes opens west, east half east, the middle lane none) applies over the S side's groups instead of a group's lanes. With two S groups, the western opens west and the eastern east. The fan is the existing vertical S-bend, sized by the solve and allowed to be 0, so the ports keep the notation spacing and the blocks spread apart as the occupancy needs. Width 1 is unchanged.
- **A flat-tile tie that may not share** (found at H1a's fourth stop: edge 4's two tracks' S-bends start on different rows, so at the flat solve copy 0's vertical TRACK_Y and copy 1's TRACK_RIGHT stand on one tile, row 10 x 39, and the occupancy family tied them): occupants on one flat tile whose bits `JunctionTracksMayShareTile` refuses are separated in their flat order, not tied, in a bundle graph and at a point (the at-point mode already had it, DESIGN-G3 F1). Not at width 1 for now: there the flat solve ties such pairs and the final solve still settles legally (9 [topology] tests move when the separation applies, LL_RR grows 62 x 25 to 66 x 29), so the premise that a kept tie always fails was false. Unifying the two is an open item, not part of H1.
- **Bundle tracks keep their side** (found at the same stop, after the tie became a separation: the flat sort put copy 0 west of copy 1 on row 10, against rows 7-9): a bundle's tracks never cross each other inside the core (REVIEW-width-overprescribed Q1, genuine). So on equal flat columns, the two tracks of one band edge sort by their side: track 1 lies right of travel, which is west of track 0 when the edge heads south (it carries exit paths, the programs' `heads_south`) and east when it heads north. Every other pair keeps today's key.
- **Stack.** In a bundle row the first BlockSide item lies at least one row past the Copy item (`ge`, not `eq`). The join block's attach pins it: cB and fB arrive 4 and 5 lines from fA. The two BlockSide items stay adjacent.
- **Occupancy.** For each hub row the block covers, there is one footprint occupant from its first to its last tile on that row. Its bits are the union of the bits of those two end tiles; every other field is as in today's footprint occupant. The tiles are the block's track pieces (bridge middles excluded) and its prio tiles.
- **Removed:** `JunctionBlockFootprint`, `JunctionBlockAttach`, `JunctionBlockFootprintOf`, `JunctionBlockAttachOf`, `JunctionSpec::exit_block_along` and `join_block_along`. `JunctionBlockSide` stays.
- **Pattern.** An Exit node takes `JunctionExitBlockProgram`, a Join node `JunctionJoinBlock227Program` (D283; the pattern toggle comes with D284).

### 7.2 Steps
- **H1a. Blocks in the core** (§7.1). `OrientJunctionBlock` and the placement change. Outer test: the width-2 T places at the test spec with no placeholder lengths, every attach and block end as §7.1. Then: core render plus oriented block tiles clash nowhere, and oriented routes rasterise to the oriented tiles in all 8 orientations.
- **H1b. The hub's routes.** One route per hub lane path, built from core track routes and block routes joined at the attach tiles. The hub signal graph is the block subgraphs plus one edge per core track, merged with the block edges it joins (port edges vanish into chains). Tags follow the block convention. The hub topology carries explicit lane paths with a per-path prio flag (the fixed tracks), which replaces D298's movement list. One movement holds both fixed and choice paths in a hub, so the flag must be per path. Then the signals run with the oriented prio spans, and the checker runs as the oracle. Expected findings go to H1c.
- **H1c. Four decks in the core** (D188, review R19): each through track crosses on its own doubled pair. The checker's D299 exemption narrows to choice legs (a bridge after the last split of every unlisted path over it). A choice track in the core has not split yet, so it is doubled (D188 "Double each lane: four").
- **H1d.** Seed and hover timing (D297), then the rig.

### 7.3 H1b: the hub's routes, graph, signals and oracle
A hub is the core's placement and render plus its oriented blocks, composed into one lane-level hub. One function in block/, `ComposeJunctionHub`, builds `JunctionHub` {layout, topology, paths, graph, programs, routes, prio}. It reads the core's graph, programs, placement and render routes (one per edge and track, tagged with the core edge) and builds the blocks as placement does.
- **Hub slots.** The core's placement uses the one-lane topology: one slot per bundle, with `track_ports` per track. The hub topology is the same topology with every group at 2 lanes. Hub slot of (one-lane slot s, track k) = the k-th lane of s's group, ranked by its port tile along the side's slot order: W north to south (y ascending), S west to east (x ascending), E south to north (y descending).
- **Hub lane paths** (D301), by walking from each hub in slot. Core In edge track k reaches its exit block at port k of in bundle 0. Each block path from that port leads to an out port; its (bundle b, track t) gives the core edge by the node's role (split: bundle 0 the straight edge, 1 the diverging edge; merge: the trunk) and its track t. That core track reaches the next block's port of the bundle its role names (merge: straight in bundle 0, diverging in bundle 1), track t, and so on to a hub out slot. Movement = the hub movement of (in group, out group). `prio` holds when the path's join-block path is on that block topology's prio list (block path index = block movement index). For LL² any this gives 18 paths, 6 with prio: per (in arm, destination), one fixed path and two choice paths.
- **Routes.** One per hub path, two for a path through the exit block's doubled l0b (copy 0, copy 1). Pieces in travel order: each core track's render pieces, then the block route's pieces (the oriented route, rasterised at the anchor tile, of the matching copy), and so on. Tags: a core piece gets {hub edge of (core edge, track), RUN, 0, copy}; a block piece keeps its block tag with the block edge mapped to its hub edge and copy set to the route's copy.
- **Signal graph.** Nodes: every block node, one per block instance (split as in the block, level 0, Plain; slot = the first hub path's in slot for a split, out slot for a merge). Edges: chains. A block edge to an out port, the core track it meets, and the next block's edge from the matching in port form one hub edge; a core track from a hub port joins the block edge it meets, and likewise at an out port. Kind: In from a hub port, Out to one, else Leg. `paths` and `path_edges` follow the hub paths. `crossings` are each block's crossings mapped, plus every core crossing (e, x) for every track pair (hub edge of (e, a), hub edge of (x, b)), sorted. `programs.edges` and `copies` are sized to the hub edges, and only the hub edge holding a block's doubled edge carries that edge's two SBend tokens, as in the block.
- **Layout.** The core render layout with every block route's raster added at its anchor. Blocks lie inside the box (block box floors), so the frame stays the placement's. Normalise as render does; the size and origin must not move.
- **Prio.** `OrientJunctionBlock` also turns the block's prio spans (merge tile and tiles by offset from the anchor, bits through the same track transform, drawing order kept). The hub's spans are those plus the anchor.
- **Oracle.** `SignalJunctionTrackRoutes` on the hub with its spans must succeed. `CheckJunctionRules(layout, spec, hub.topology)` then runs, and its findings are written down, not patched (D193). The expected finding is "ML bridge is not doubled" on the core's through-track bridges, which H1c fixes (four decks, D188). Any other finding is a design gap for the orchestrator.

### 7.4 H1c: four decks in the core (D188), by strands
RECON-four-decks.md maps the code. An edge's **strands** are its physical tracks. Strand s = copy x W + track, where W is the edge's width. This keeps today's meaning of index 1 everywhere: at width 1 a doubled leg has strands 0 and 1 (copy 0, copy 1); at width 2 an undoubled bundle edge has strands 0 and 1 (tracks 0 and 1, `programs.copies` = track 1); a crossed width-2 through edge has strands 0..3 (track 0, track 1, track 0's copy, track 1's copy). Programs for strands 2 and up live in a new per-edge list (H1c3), so `edges` and `copies` keep their meaning. Across travel the order is [track 0 copy 1, track 0 copy 0, track 1 copy 0, track 1 copy 1]: copies outward, so no track crosses another.
- **H1c1 (storage, identity).** In placement, `chain[e]` and `chain_copy[e]` become `chains[e]` (a vector of chains, one per strand; strand 0 is today's `chain`, strand 1 today's `chain_copy`) [DONE 2e6922fb10]. Every two-element loop iterates the strands. `two_chains(e)` becomes `chains[e].size() > 1`. `nth_heads` takes a strand. `JunctionTrackPlacement::bridges` becomes a vector per bridge, indexed by strand, with readers adapted. `Occupant::copy` becomes the strand (the shared part stays -1). Programs, render, signals and the hub are unchanged except for the bridges type. Width 1 and the bundle T produce identical bytes.
- **A track's doubled pair** (equal pathfinder cost, as the width-1 parallelogram): the leg starts and ends on track k's own row a (exact block ties), and its doubling copy row b = `track_copy[k]` lies outward. Strand k (copy 0) is: Over..., SBend(LaneMerge) to b, then a shared SBend (role None) back to a. Strand W + k (copy 1) is: SBend(LaneSplit) to b, Over... So each copy bends once before the merge, as at width 1 (with exit item := a, lane item := b), and the merged track returns to its row before the join block. The shared return bend is on strand k only, as the width-1 lists keep shared parts on copy 0.
- **Band bridges stay in their band** (found at H1c4's stop: edge 8's bridge over edge 4, a band 2 crossing, had its north heads inside row 1's four-deck rows, where the clearance family can satisfy neither side): a single bridge of a band crossing event in band b has both heads south of row b - 1's last stack item and north of row b's first, by one row (the band-gap rule the band nodes already have). Applied in every graph. If width 1's identity moves, it is scoped to bundle graphs and recorded.
- **H1c2** stack items per track copy in a crossed bundle row (copies outward). **H1c3** programs per strand (`programs.copies` generalised to strands). **H1c4** one per-strand doubled-leg relations block (unifies the width-1 doubled-leg block and the bundle row block). **H1c5** render, `JunctionRouteEvents` per track, and the hub per (edge, strand). A hub path may pass both a doubled core track and a doubled block edge; its routes are the product of the copies (the hub refusal at junction_hub.cpp:390 goes).
