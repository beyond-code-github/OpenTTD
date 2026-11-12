# FABLE-projection: review of DESIGN-G1 §13 "Projection on the graph" (at 19bdc4baa7)

Read-only review. Read: DECISIONS D1-D213 (D182-D213 closely), my FABLE-nodes-perf, -topology-graph, -G1, -G1c, -G1c2; DESIGN-nodes, DESIGN-N3, DESIGN-G1 (whole); RECON-seed-projection (RP1); briefs/RP2; STATUS 1-3; the NP1 (a866725b2f) and NP2 (a91b188640) diffs. Code: src/junction_nodes.{h,cpp}, junction_positions.h, junction_topology.{h,cpp}. Line numbers are junction_nodes.cpp unless a file is named.

## Verdict: sound with changes

§13.0-§13.4 are right. The template input comes from the graph pipeline. Pieces are tagged with graph elements, rules sit on those elements and failures name them. That is D201/D213 as ruled, and it repairs the NP3 drift.

**§13.5a is not right and should be withdrawn.** It changes D198's best-first order to depth-first. The only reason given is that an incremental template avoids rebuilds, and that reason does not match the code:
1. Template build is less than a tenth of a state's cost. RP1 measured the T at 0.15-0.19 ms to build against 1.7-2.0 ms to solve.
2. The solve is global and greedy. The Y snap runs over every Y class (:1380-1400), then come the late constraints (:1402), the occupancy chain over all rows (:1409), the X facts, and the X snap (:1525). An edit anywhere means running all of that again, with or without a template trail.
3. Class numbering is global. Union-find runs over all points, X classes are numbered before Y classes (:268-280), and many rules span several edges.
4. D213 itself says "the state is placed and solved again".

So the trail would save very little, and getting it would mean rewriting NodeBuilder, the most fragile code in the tool. Keep best-first and rebuild each state from its edit list. Measure, then optimise only what the measurement shows.

## Q1. Consistency with the rulings

- **Consistent:**
  - D201 and D213 (the graph is the source; the layout is only output).
  - D182 and DESIGN-nodes §2 (obstacles add structure).
  - D193: failures are named and repairs are edits.
  - D194: no trial and error, provided children come only from named failures.
  - D211 and D212 (rules on tokens).
  - §0b: P1's oracle.
- **Conflict 1, D213:** "after which the state is placed and solved again". §13.5 says "the search does not rebuild or re-solve the whole template per child". These cannot both hold. D213 is the ruling.
- **Conflict 2, D196/D198:** "ranked in one queue", "stop at the first solved state no queued bound can beat". Depth-first with a node cap returns the first solution under the cap, not the cheapest. §13.5a calls itself an amendment, but its premise is wrong (verdict), so the amendment should not go to Pete as worded.
- **Gaps:**
  - **D199:** the user weight between organic and artificial building is not in the cost or the estimate.
  - **D197:** cursor offsets: §13 never says where JunctionNearOffsets sits. Ruled: outside the cost queue, used only when the cursor spot has no layout.
  - **D189/D190:** crossing assignment is a situational choice, with main line over first. §13.5 lists over/under and bridge/tunnel among the repairs; see Q4 on why these must not be ordinary children.
  - **DESIGN-G1 §0:** "no constructed layout anywhere on this path". §13.1 takes routes from RenderJunctionTrackPlacement, which also builds a JunctionLayout (junction_topology.cpp:1665). Q2 covers the split this needs.
  - **D193:** the post-solve checks JunctionNodeHeightsOk and the fit loop run on the rendered layout inside the search (:1753, :1760). §13 does not say whether they stay or how their failures name graph elements.

## Q2. Source and tags (§13.1-13.2)

- **Is "routes at placement tiles" legitimate?** Yes. As I wrote in FABLE-topology-graph §1 and §5.2, the solve needs a reference embedding in any case: tile identity, row order, over/under, extremes and sides, plus the snap target. D202 made those reference positions part of the graph. Pieces computed from programs plus placement are that reference, computed by maths.
  - It turns into a disguised layout only where the input is re-derived from the tile union instead of read from the graph.
- **Re-derivations that exist today:**
  - (a) `events` come from SwitchEventsOf(layout, used, path) (:684).
  - (b) The builder's prio detection is tile coincidence: another path's merge on the same tile with a different trackdir (:355-365). `joiner_row` comes from the reference tile before it.
  - (c) The signal-token pass, which §13.1 names as the slot source, itself reads the layout:
    - SwitchEventsOf over layout tiles at junction_topology.cpp:1861, :1952, :2017, :2073;
    - the joiner is found as "the first route of another movement with a piece on M" (:1863-1876);
    - IsLegalSignalPiece/IsSignalledPiece on the layout at :1969 and :2046.
  - So "slots from the signal-token rules" brings the layout read back unless ST1's event source is changed as well.
- **What P1 must do:**
  - Split the route builder into "routes (tagged pieces)" and "draw routes into a layout" (output and tests only).
  - Take events from graph nodes: placement.nodes plus the doubling lane split/merge decorations, which are not graph nodes. Their tiles are copy 1's SBend first and copy 0's SBend last (§11.10).
  - Take prio merges from a merge node whose in-edges carry different movements.
  - Take sides from the structure (§9: the prio opposite the joiner).
  - Take slots from a token-rule function over routes plus nodes, with no layout argument.
  - `used` can be dropped: only SwitchEventsOf reads it (:679, :684); NodeBuilder never does.
- **Tags that make failures useful:**
  - A free link always lies inside one edge between two consecutive tokens, because switch pieces are points. Tag it (edge, run k, copy). Straight/Break/Edge failures (h:188-192) then name exactly the run an S-bend edit needs.
  - A point gets (edge, token) or (node). A bridge gets (structure bridge, copy).
  - A class gets the set of its points' tags. Piece and Height failures name a class, so the failure must also carry the failing piece (HeightFault.tiles to piece to tag), not just the class.
  - Occupancy today reports only a row (h:191). It must report both elements of the overlapping pair.
  - Template errors must name the element, not "path i".
  - The anchor and NodeAnchorShift (:1694, T roles) must go. Use the placement's port slots, which are known exactly once tags exist (RP1 gap 4). §13 omits this, and LL_RR cannot be offset without it.

## Q3. The D212 lean (§13.3)

- **Direction:** right. An in-fan moves downstream and an out-fan upstream, at its extreme within the propagated bounds. "Limited only by other tracks" then falls out of the occupancy chain and the curve and slot rules, so the minimum comes from the rules and is not hand-coded. That is the generic encoding D212 asks for, and better than FN1's d_min.
- **Failure modes:**
  - **(a) Phase.** "Snapped after all other classes" must be per axis. A lean on a Y class (a port on the N or S side, or a later N-S main line) belongs at the end of the Y snap, before the late constraints, not after the X snap.
  - **(b) Shared classes.** A Fan's class can hold more than the fan, through rigid links next to a bridge head (FABLE-G1c2 F1) or a short fan tied to a node. Leaning it would move a node, which D212 forbids ("the seed does not slide").
    - The rule should be: a lean class holds only the Fan token's own pieces. Otherwise no lean, and the case is listed in a test findings file.
    - §13.3 only excludes the class shared with the port.
  - **(c) Order between leans.** Two leans compete for the same slack (the rig finding was the top two lanes). Greedy order then decides which lane keeps its row. Fix the order by rule, e.g. outermost lane first, and test both W lanes of LL_RR.
  - **(d) Y snap blindness.** Leans see only the rows the Y snap already chose. A Y move made for a fact can leave a lane with no room to lean; that is acceptable, but should be reported as "held by" a named element.
  - **(e) Generic anchor.** Fan tokens exist only for W and E main lanes (§10.7). Phrase the rule as "the first bend token after an in-port / the last before an out-port on a port edge", so that a multi-lane branch later is covered.
- **Out-fans:** they "already comply" on flat ground only because placement made them exact (§11.8). In the template the merge-to-fan run is free, so the out-fan lean is needed on terrain. Keep it.

## Q4. Search (§13.5a)

- **Hover with warm start.** The cost per state is placement, routes, build and solve. Estimate for LL_RR: seed pipeline 1.8 ms (RP1), template build about 0.5 ms (INFERRED from the T's 0.19 ms scaled by points), solve about 4-6 ms (INFERRED). That gives roughly 10-15 states in 100 ms (INFERRED). Failure-driven children (D198: 1-3 per failure) keep the best-first frontier small, so best-first fits the budget with no trail.
  - Depth-first with a cap risks spending the budget in one deep subtree. That subtree could be terraform-first if the estimate ranks it so.
  - If the measurement shows the first incumbent arrives too late, add a single depth-first dive from the root to get an early incumbent. Keep best-first as the order.
- **Admissible bound:**
  - Foundations are not committed before a state's solve: positions decide which tiles carry them. Their lower bound is 0, or their exact value after a successful solve.
  - Track can be bounded by the sum of each link's constraint minimum (k of rule 1 or a curve-gap floor), plus the rigid pieces.
  - Terraform overlays already applied count exactly.
  - This bound is admissible only if every edit below a node is **additive**: S-bend, ramp, terraform. Swaps (bridge to tunnel, main line over to under, crossing realisation, order or doubling change) can make a subtree cheaper than its parent, so pruning below them is unsound.
  - Fix: enumerate swaps as roots, one per crossing case (D167, D189), with main line over first (D156, D190). Put additive repairs below them.
- **Incremental template:** not workable as written. Each of these breaks "re-add from the first touched edge's mark":
  - An S-bend adds points, splits a link and creates a class. A new X class renumbers every Y class (:274-279).
  - Curve gaps pair curves up to a train length apart, across nodes. Prio coverage goes 2p units back across edges. Crossings pair a bridge edge with a crosser edge. Occupancy sorts whole rows.
  - Even if all that worked, the global Y snap / occupancy / X snap still runs per state.
- **What works:** a state is an edit list over the graph (position-free, see Q5). Each state is rebuilt from the earliest stage its edits touch (Q5), then built and solved. The JunctionPositions trail stays inside one solve, where it pays off already (snap probing, :1091-1108).

## Q5. Repairs as graph edits (§13.5)

- **Local** (the edit replaces the interior of one edge's run or span, keeps its endpoints, and only routes through that edge change):
  - an out-and-back S-bend inside a free run: InsertJunctionDetour moved from layout to routes, with the same collision check against the reference;
  - RS1's ramp inside a diagonal span (DESIGN-N3 §11, FABLE-topology-graph §4: the edit must compute reference tiles; "no positions chosen" is not available).
  - Restart point: the routes of that edge. The template is then rebuilt from those routes.
- **Not a graph edit:** terraform (a fact overlay). The template is unchanged; re-solve only, or the local check of DESIGN-N3 §11.
- **Upstream of placement:**
  - an S-bend that **stays shifted** past a node (D182, D195: "merge on row 1"). It changes a node's row and the stack items next to it;
  - bridge to tunnel, and main line over to under (programs: Over/Under carriers; placement: the head rules);
  - a bridge cut (CutJunctionTrackBridges is itself a search over placements);
  - node moved to another edge, order change, doubling change: back to G1b(ii) or earlier.
  - Restart point: the stage that edit touches. Incrementality across stages is not needed at these sizes (the LL_RR cut search ran 6 placements in under 1 ms, §11.9b).
- **Edits must be position-free:** (edge, between tokens i and i+1, side, kind), with columns left to the solve. Otherwise a warm start at a new cursor cannot reuse them.
- **Staleness:** edits whose failure no longer occurs still add cost. Warm start should solve the bare graph and the stored edit set, and keep the cheaper (§13.6 says only "re-check its facts").
- **Today's search:** it repairs only Straight and Break (:1774). Height, Piece and Occupancy failures have no repair kind yet. §13 should list which failure kind maps to which edit, and state "no repair, report" for the rest.

## Q6. Missing

- **Signals after repairs.** An inserted S-bend changes slots and bays on its edge (G7, D176). Slots must be regenerated by the same token rule for every edited edge (Q2). Template rules 4-6 read them (:492).
- **Prio.** The prio row is a reserved occupancy element (DESIGN-N3 §8), and coverage rule 3 forbids curves inside it. An S-bend repair inside a coverage fails by rule. That needs a named failure, "coverage of merge m", that the search does not try to repair, rather than a Template error.
- **Y snap blindness (DESIGN-N3 §6).** The greedy nearest-flat Y snap with no backtracking (:1380-1400) is a search hidden in the solve. On LL_RR, with 12 adjacent stack rows (D142), a Y move made for a height fact will show up later as an X-phase Occupancy failure that names the wrong element.
  - P4 must measure this.
  - If it occurs, the failure names the Y class, and "Y class c to its next row" becomes one repair child.
- **Offsets, box, ports.**
  - Replace NodeAnchorShift and the exit-path anchor (:712-723 after NP1) with the port slots. This is RP1 gap 4.
  - JunctionBlockedPortApproach assumes the ports on row 0 (junction_game.cpp:461). This is RP1 gap 8.
  - Return ports with tile, heading and approach for the line painter (FABLE-nodes-perf §3.4). D212 makes the approach run part of the line.
- **D193.** Keep CheckJunctionRules as a test oracle on every P-step output. List the hover-path acceptance checks (:1753, :1760) as uncovered rules, each to be moved into the template.
- **Old T path.** GenerateJunctionNear still uses the traced test-spec T (junction_game.cpp:446, JunctionNodeTopologyT). Under §0b the hover moves only when the graph-built T passes the existing N3c search cases (house, hub tile, W-end incline) and [nodeoracle] accepts at least its last count. P5 does not state this gate.
- **Oracles.**
  - (1) After NP2 the LL_RR seed traces (test at tests/junction_topology.cpp, "the LL_RR seed builds a node template"). So the graph-built LL_RR template can be compared with the traced one, not only "builds".
  - (2) The render, retrace and compare validator on **every edited state** in tests (FABLE-topology-graph §5.3). Edits compute reference tiles, and nothing else checks that they are consistent (diagonal endpoint, collisions).

## Q7. Steps and RP2

- **Order:** P1 to P3 are right. Changes:
  - P1's outer test: DiffJunctionNodeTemplates(graph-built, traced) for **both** topologies, compared canonically. Bridge order and diff order differ only in order (RP1 Q1). It also replaces the anchor.
  - P2 removes the solve's PlaceJunctionSignals call (:1536) and ports SignalJunctionTrackRoutes' event source to the graph.
  - P4 is too big. Split it:
    - P4a: tagged failures, including post-solve checks and occupancy pairs;
    - P4b: best-first rebuilt per state from edit lists, with the local S-bend edit (house on a fan, D195 block);
    - P4c: RS1 ramp, needed for the ridge test (D196, D200 RS1 before TF1). No step currently builds it.
  - A minimal P5 (hover by topology, offsets per D197, no picker) could come straight after P3, so Pete sees LL_RR follow the cursor early (D197 lets him choose the spot). Recommended, not required.
  - Every P-step keeps the §0b gate.
- **RP2:** run it, with additions:
  - Q4 and Q5 must also cover SignalJunctionTrackRoutes' own layout reads (junction_topology.cpp:1861-2073) and the doubling lane split/merge events.
  - A question on the builder's tile-coincidence reads (:355-365, the flat_bits union in the solve, the occupancy flat order) and their graph replacements.
  - LL_RR as well as the T in Q2 and Q8: it now traces.
  - **A Release timing of one LL_RR state:** placement, routes, build and solve split out. This settles Q4 with numbers.

## Q8. NP1 and NP2

- **Keep both.**
- **NP1** replaces the T-role tests with movement flags from the topology (`through` and `exit`, h:113-114; :351-365, :519, :529). The graph producer fills these from the topology in one line each.
  - Caveat: the anchor is now "first exit path" (:712-723), which depends on path order. P1 replaces it.
  - Also keep NP1's topology overload of TraceJunctionNodeInput, as the LL_RR oracle.
- **NP2** gives per-link signs. They are more correct than per-path signs and generic. The unit formula with per-link Y terms is a valid X difference only once the Y values are known (late gaps). It correctly fails a static gap with mixed Y signs and no X run, since that is not a two-variable difference.
  - `JunctionNodePath::dir`/`dir_y` are now read only by the comparator; they can go later.

## Changes to §13

1. Withdraw §13.5a's depth-first order and incremental template. Keep D198/D196 best-first and rebuild each state from its edit list, as D213 itself says. Optimise only after the RP2/P4 per-state timing.
2. §13.5: delete "does not rebuild or re-solve the whole template per child". Replace with a table: edit kind, the stage it restarts from (routes, placement, structure, order), local or upstream (Q5). Terraform is a fact overlay, not a graph edit.
3. Bound: a lower bound from track per link minimum plus rigid pieces, plus applied terraform; foundations 0 until solved. Additive edits only below a node; swaps (over/under, bridge/tunnel, crossing realisation, order, doubling) enumerated as roots, main line over first. Add D199's user weight to the cost and the estimate.
4. §13.1: no layout on the input path. Split the route builder from drawing. Events from graph nodes plus the doubling decorations. Prio merges from merge nodes with in-edges of different movements. Slots from a token rule over routes and nodes. Port SignalJunctionTrackRoutes off SwitchEventsOf(layout). Drop `used`.
5. §13.2: tag links (edge, run k, copy), points (edge, token | node), bridges (structure bridge, copy). Failures carry the failing piece's tag. Occupancy names both elements. Template errors name elements. Replace the anchor and NodeAnchorShift with port slots.
6. §13.3 lean: per-axis phase (Y leans at the end of the Y snap); a lean class holds only its Fan's pieces, else no lean and a finding; a fixed order between leans (outermost lane first); anchored on the first bend after an in-port / last bend before an out-port.
7. §13.4: say what happens to the post-solve checks (:1753, :1760). Either encode them in the template (D193) or have them name graph elements. Add the Y snap's misattributed Occupancy failures as a P4 measurement, with a "Y class to next row" child if they occur.
8. §13.6 warm start: edits position-free; solve both the bare graph and the stored edits, keep the cheaper; terraform overlays re-derived, not reused. Offsets (D197) only when the cursor spot has no layout.
9. §13.7:
   - P1's outer test: canonical template equality for the T and LL_RR, against the traced oracle.
   - P2 removes PlaceJunctionSignals from the solve.
   - Split P4 into a/b/c, with RS1 in P4c.
   - P5 gated by §0b (N3c cases plus [nodeoracle] count); optionally a minimal P5 after P3.
   - The validator (render, retrace, compare) on every edited state in tests.
10. Run RP2 with the additions in Q7, including the per-state LL_RR timing.
