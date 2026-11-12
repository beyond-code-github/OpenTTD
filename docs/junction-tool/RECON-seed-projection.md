# RECON-seed-projection (RP1): the G1 seed through the node template and solve

Read-only recon at dd31cc9659 (worktree .claude/nodes, branch wt-rp1). A hidden diag TEST_CASE measured, then was removed. Line numbers are src/junction_nodes.cpp unless named. Release = build-rel. Nothing marked INFERRED was run.

## Q1. Does the seed go through today?
- **T seed: yes, completely.** SignalledOf and RenderOf (junction_topology.cpp tests :758, :1204) give the T. `BuildJunctionNodeTemplate(layout, spec)` builds with `error == ""`: 62 points, 63 links, 7 paths, 81 pieces, 3 bridges, 33 joins, 5 diagonals, 20 X classes, 32 variables, open_joins 0, 68 diffs, 2 prio rows, anchor class 2.
- **Same for the signalled seed** (184 tiles, 92 signals): the template is identical to the unsignalled one. The signal pass adds 30 tiles (prio-element zigzag, tile bits 9/5 and 33/17) and signals that are not path pieces.
- **Diff against the traced flat T** (ConstructJunctionTMap, test spec): `DiffJunctionNodeTemplates` first reports `bridges[0].bridge.start.x: 23 vs 26`. That is list order only: the seed lists bridge (23,3)-(29,3) first, the flat T lists (26,6)-(26,9) first. After equalising bridges, the first diff is `diffs[43]`, again order only: both have 68 diffs and are equal as multisets of (a, b, k). Points, links, paths, pieces, joins, diagonals, `flat`, x_classes and anchor are equal. **The seed's template is the traced T's template** (stronger than the old counts test, :767).
- **SolveJunctionNodes at offset 0 on flat facts** (flat Buildable terrain, same shape as tests/junction_nodes.cpp:226): feasible, `values == tpl.flat`, rendered tracks have **0 tile diff** against the seed (compared normalised). Unsignalled and signalled seeds both pass.
- **Signals after the solve:** `solve.layout` (render, then PlaceJunctionSignals :1508) equals the signalled seed in tracks (184 tiles, prio element included) but not in signals: 90 vs 92. Differences: solved-only (2,3 face 13) and (3,4 face 0); seed-only (1,3), (2,4), (4,4), (23,2). Both layouts have 0 violations under `CheckJunctionRules(layout, spec, JunctionTTopology())`. The solved layout equals the old flat T in tracks and signals.
- **Release timings:** seed pipeline 0.42 ms (0.49 ms with signals); template build 0.15-0.19 ms; solve 1.7-2.0 ms.
- **LL_RR seed: no.** Seed pipeline 1.8 ms (2.4 ms with signals), 451 tracks (511 signalled), 13 bridges.
  - `BuildJunctionNodeTemplate(layout, spec)` fails with `bridge 0: a head is not a piece of any path` (0 paths): TraceJunctionNodeInput :656-669 uses legacy `FindJunctionPorts(flat)` and `JunctionTMovements()`.
  - I rebuilt the input through `MakeJunctionTopologyCheckContext` (15 paths, from_slot/to_slot set) and the same UsedTracks, SwitchEventsOf and slots code. The builder then gets further (170 points, 175 links, 250 pieces, 83 variables) and fails with **`path 0 moves both ways along y`** (:256).
  - Path 0 is W slot 0 to S slot 5. Its rows run 4,3,2,1 (port row up to the lane's stack row, a free fan diagonal), then 1 down to 24 (exit Corner and Y run). `dir_y` is a single sign per path (:254-258, header `JunctionNodePath::dir_y`).
  - So the LL_RR solve was not reached. The blocker is both the roles and the one-sign-per-path rule, and the one-sign rule is exactly the D212 fan.

## Q2. Role-specific code
- **junction_nodes.cpp (path roles, EIn/WIn/EOut/WOut/SOut):**
  - `IsMainLinePath` :111-114, used at :360 (prio merge detection: a merge on a main-line path where another path with different from/to joins) and :538 (rule 6). Replace by `IsThroughPath` (both movement groups `main`) for :360, and `!IsThroughPath` for :538. "Different from/to" becomes `!SameLanePath` (same from_slot and to_slot, a lane merge has no prio).
  - Rule 5 :528 (`EIn/WIn -> SOut`, waiting room from the first split to the first merge): replace by `IsExitPath` (main group to non-main group).
  - Anchor :687-695, AddJunctionNodeConstraints :705-706 (pins the EIn->SOut path's last X class and first Y class to flat). With no such path the anchor is UINT16_MAX: no pin and no error. A topology-neutral pin (box NW corner, FABLE-G1c §3(c)) is needed only if a translation reference is wanted.
  - `NodeAnchorShift` :1666-1675 (rebuilds a legacy context to find the EIn->SOut piece) is used by SearchJunctionNodes :1691, :1717 to turn the normalised layout back into a map offset. Without the anchor the search returns "no anchor path" (:1692).
- **Role code outside junction_nodes.cpp on the solve/render path:**
  - `JunctionNodeHeightsOk` :1516 (legacy ctx).
  - `PlaceJunctionSignals` :1508 -> junction_signal_pass.cpp `IsMainline` :35, FindJunctionPorts / JunctionTMovements :617-620, :642-643.
  - junction_rules.cpp `IsThroughPath` :152, `IsExitPath` :160 and `SameLanePath` :168 are **file-static**; they are not exported.
- **Not role-based but T-shaped:** one x-sign and one y-sign per path (:254-258); `JunctionBlockedPortApproach` (junction_game.cpp:461) assumes ports in columns on row 0.

## Q3. FABLE-G1c §3 (a)-(f) today
- **(a) Bridges rigid, columns at minimum spacing:** solved by exact placement (DESIGN-G1 §11.8); diag: bridge classes and diffs equal the traced T's.
- **(b) Lo of the early system is not a layout:** still true (diag: with only the anchor pinned, 19 of 32 classes have Lo != flat, many at -1024). **It no longer matters for projection**: the solve snaps to the seed (flat) via the nearest accepted value (:1060, :1497), and the seed is the placement's least solution. A Lo/fixed-point step is a validator, not a requirement.
- **(c) Pin the NW corner rather than free the anchor:** applies only if Lo is wanted. For projection, only the offset reference matters (Q2, NodeAnchorShift).
- **(d) Seed choices frozen into the reference:**
  - Occupancy gap `min(2, flat gap)` :903: still there; harmless while the seed is exact.
  - Notation spacing at ports: AddBoxEdges :630-651 ties only the extreme end points; spacing is held only by snap-to-flat. Still applies.
  - Joiner-row sign :367, slot set (`input.slots`) and flat_bits: read from the seed; slots are identical for signalled and unsignalled seeds (diag).
  - Role code: Q2.
- **(e) D80 equal copies as a rule:** equal span holds by rigidity (each bridge's heads share one class). Diag: the two W bridges (X classes 11 and 10, one Y class 26) are tied only by `10 - 11 >= 1`; the D118 stagger of exactly 1 is not a rule. Applies (stagger only).
- **(f) Compare templates modulo flat and AddCrossings sets:** done for the T by the multiset diff above. The LL_RR comparison needs Q2 first.

## Q4. Signals
- The node path drops the seed's signals. SolveJunctionNodes renders tracks (RenderJunctionNodeTracks, "no signals and no prio rows") and then calls PlaceJunctionSignals (:1508). That is the legacy T pass (junction_signal_pass.cpp:617-643).
- The seed's SignalJunctionTrackRoutes tokens and prio element are not read: slots are IsLegalSignalPiece/IsSignalledPiece (:656-669). The prio element is re-laid by `ReservePrioJoins` from the T's roles; for the T it came out equal to the seed's (184 tiles).
- GenerateJunctionNear does nothing for signals after the solve; the layout it returns is the search's `solve.layout` (junction_game.cpp:478). SizeJunctionPrio (junction_generator.cpp:1949, :1964) does the same render, then PlaceJunctionSignals.

## Q5. The port fan (D212)
- A lane's port-to-fan run is a **free X link** (same row, X trackdirs, no bridge piece: header `JunctionNodeLink::free`, :195-224 builder). The fan S-bend is a **free diagonal** when it has 3 or more halves (rigid when shorter, or under a Y-axis bridge, per DESIGN-G1 §11.1).
- The solve picks, for each Y class, **the nearest value to flat** that meets the facts (`snap`, :1060-1076: tries flat-d then flat+d, low first); X classes likewise after Y (:1497). So a free run lands where the seed put it, moved only by terrain or by constraints. `JunctionNodeSnap` (:918) clamps to flat the same way.
- On terrain the fan lands exactly where the seed has it today. The seed's least placement puts a fan early (D212's complaint).
- **Existing mechanism where "fan as late as it can" could go:** the snap target `tpl.flat[c]` (:1060, :918) is the only preference; there is no cost or objective in the solve. The structural alternative is the seed's free-run lengths.
- A fan diagonal's two Y classes also give `dir_y` both signs on a lane that fans one way and exits the other (Q1).

## Q6. Terrain and obstacles (solve) and T dependence
- **Heights/foundations (HF1, D194):** facts `Fits`/`EdgeZ`/`Straight` from the game's foundation tables via JunctionNodeTrackFits (:1086-1090), joins (`join_meets` :1079), Kind::Height with fault info. Template-based (`tpl.joins`), role-free. `JunctionNodeHeightsOk` (:1516) is T-context (Q2).
- **Houses/obstacles:** Blocked tiles in JunctionFacts (`buildable` from `cls`, junction_facts.cpp:47). Piece fit (step 3), diagonal interiors (DF1), prio-row tiles (PF1) are role-free. The prio row depends on `prio_rows`, built at :459-460 from the main-line role (:360).
- **Negotiation/detour (InsertJunctionDetour :1552, SearchJunctionNodes :1677):** detours replace one free link's interior; `Straight`/`Break` failures only (:1746). T-role dependent through NodeAnchorShift and BuildJunctionNodeTemplate on each state.
- **Offsets:** JunctionNearOffsets, rings of 2/4/6/8 (junction_game.cpp:417); role-free.
- **Tunnels (G5):** junction_nodes.cpp has no tunnel handling; `JunctionBridge.tunnel` is carried only. G5 lives in GenerateJunction (old generator), not in the node solve (STATUS: "G5 MERGED ... SL tunnels", node solve untouched).
- **Terraform:** none in the node solve (TF1 queued, STATUS :18, :25, DESIGN-N3 §11 draft).

## Q7. Wiring
- No topology reaches the game. GenerateJunctionNear (junction_game.cpp:441) takes `const JunctionTMapResult &topo = JunctionNodeTopologyT()` (:446): the old traced flat T, a function-static built once with MakeTestJunctionSpec (junction_generator.cpp:1936-1939), cached by this static, not by the GUI.
- It then shifts the terrain (:458), computes `start_offset = (frame.hub_x - topo.hub.x, -topo.hub.y)` (:459), checks D179 (:461), and calls `SearchJunctionNodes(*topo.layout, spec, shifted, start_offset, 16)` (:472). `hub` also comes from `JunctionTMapResult`.
- The GUI calls it from junction_gui.cpp:659 with a spec from MakeJunctionSpec (ml_gap/branch_gap notation; JunctionSpec has no topology field). The only users of JunctionTTopology/JunctionLLRRSidelineTopology and the placement/render pipeline are tests and the rule checker (junction_topology.cpp, tests).
- The flat T is built at the test spec (fixed prio, tl); the template is rebuilt with the real spec (:1696ff), so today a larger spec stretches the test-spec seed by the template rules. A seed from the placement is per spec (CL, TL, prio_tiles: DESIGN-G1 §11).

## Gaps for projection of the G1 seed (by dependency)
1. **Topology-aware JunctionNodeInput producer.** TraceJunctionNodeInput :656 is the only producer and is T-only. Build it from `MakeJunctionTopologyCheckContext` + `FindJunctionPorts(layout, topology)`; the diag shows this gives 15 LL_RR paths and a builder that reaches the path loop.
2. **Path direction rule.** Allow a path to move both ways along y (and x): `dir`/`dir_y` single sign per path (:254-258, used in rules 1-3, AddCrossings :606, RenderJunctionNodeTracks). Needed by every lane with a fan; blocks LL_RR first. Needs design.
3. **Role replacements in the builder:** :360 -> IsThroughPath/SameLanePath, :528 -> IsExitPath, :538 -> !IsThroughPath. Export those three from junction_rules.cpp (currently static). Add topology to the template inputs so these can read movement groups.
4. **Translation reference:** replace the EIn->SOut anchor (:687, :705) and NodeAnchorShift (:1666) by a topology-neutral reference (box NW corner or a W port piece), so SearchJunctionNodes can map a normalised layout back to an offset.
5. **Role-free height check and signals:** JunctionNodeHeightsOk :1516, PlaceJunctionSignals / junction_signal_pass.cpp:35, :617-643 take the topology (or use SignalJunctionTrackRoutes tokens from the seed's own routes, Q4: the T's prio element already matches).
6. **Solve LL_RR on flat** (expected feasible once 1-4 hold; not run). Then the T is the regression.
7. **D212 fan landing:** projection-time preference for a lane to keep its port row. The solve has no objective; the only lever is the snap target (:918, :1060) or free-run lengths in the seed. Needs design.
8. **Wiring:** a topology on the GUI/spec path; replace the JunctionNodeTopologyT static (junction_generator.cpp:1936) by the seed pipeline per spec (0.4-2.4 ms, within the 100 ms hover); hub/start_offset and JunctionBlockedPortApproach (junction_game.cpp:459, :461) from the placement's ports instead of the T's row-0 columns.
9. **Not in scope of node solve today:** tunnels (G5) and terraform (TF1).
