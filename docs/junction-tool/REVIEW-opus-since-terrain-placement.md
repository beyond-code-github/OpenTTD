# REVIEW-opus-since-terrain-placement: review of D213-D219, DESIGN-G1 §14-§17 and steps TL1b..N1 incl. the R7 WIP (at 14e6372aa9; R7 WIP 52e60a4224)

Read-only review (Opus; earlier "FABLE-*" reviews also ran on Opus). Read: FABLE-terrain-placement; DECISIONS D182, D193, D195-D199, D213-D219; DESIGN-G1 §14-§17; STATUS line 2; RECON-row-chain, -joint-levels, -slope-physics, -hover-switch, -prio-stretch; briefs R7, N1-recon, Z2c, T1c1/T1c2/T1d/H3 amendments. Code: src/junction_topology.cpp (J, at HEAD), the R7 WIP diff in .claude/nodes (W = its junction_topology.cpp, WT = its tests), src/junction_levels.cpp (L), src/junction_terraform.cpp, src/junction_hover.cpp, src/rail_cmd.cpp, src/junction_rules.cpp. Nothing built or run. INFERRED = reasoned, not run.

## Verdict
The plan holds in direction (graph -> placement on facts -> levels -> render/signals -> hover), and the work since the last review is mostly sound: the families (T1c), the joint level solve (Z2e) and the hover wiring (H1-H4). Two assumptions it rests on have broken, and R7 is the first step to hit both:
1. **"The relations are complete" (D193) holds only for unedited graphs.** The occupancy family takes its order, its ties and its legality from a solution (each state's flat solve). Legality is whitelisted by *edge pair* (J:1999, J:2008). So an edit that makes two lines meet can pass as legal.
2. **"A state costs ~0.5 ms" (§14.6) is false.** A structural edit re-runs the bridge cut and re-derives m (R7: 60-70 ms). The level solve is 7-10 ms mean, so it is the floor of every full solve.

§17 (snap and negotiate) is a good idea for local shape and node-spot choices. As written, though:
- its premise ("lengths already optimal") is false in terraform mode;
- its heuristic is not admissible;
- its candidates are not independent: they couple through rows (stack items) and through heights (ADJ).

It needs the restructure in Q2 before it is built.

**Top 5 changes**
1. **Occupancy legality without a reference.** A shared tile is legal only by a tile-local rule: the pieces are related by the graph (extend `related`, J:1988), or no two unrelated pieces share a track end (the existing geometric test). Remove `ref_shares`.
   - First explain the e12/e17 (28,11) case that motivated the reference (T1c2 Amendment 2).
   - Then make the solution-derived choices explicit per-state variables, computed at the root and carried through edits: occupant order, ties, hug side, clearance side.
2. **R7 fix.**
   - The Shift item goes *adjacent* to the edge's own item, as the brief said. Inserting it there is a row gap; the side items move out.
   - ApplyJunctionShifts places with the parent's cut and choices. New tokens start at their floors. No re-cut and no m re-derivation.
   - Restore the brief's strict test (no WARN, no loosened box bound).
3. **An edit-soundness sweep (the D193 oracle for edits) before T4b/§17.** Random Shifts, row gaps and m +/- 1 on flat ground, each checker-clean. Until it is green, keep CheckJunctionRules in hover acceptance (D193 allows this until RC0). PlanJunctionHover has no checker today (src/junction_hover.cpp:17-50).
4. **§17 restructured, with Shift as a candidate only where it is local** (details in Q2):
   - node spots are the variables;
   - edge shapes are costed links between chosen spots;
   - conflicts branch as in conflict-based search, re-tracing only the item that was constrained;
   - corner moves are charged 1/4 per touching tile, so h stays admissible.

   Shift is a negotiation candidate only on the stack's outer side; elsewhere it is structural.
5. **Height model and ramp rule.**
   - Model terrain corners as variables with |delta| <= 1 between axis-adjacent corners. This is the game's own propagation, exactly; it replaces SLOPE2 and the spill failures.
   - Fix the ramp rule: windows across bridge middles, windows counted in units not visits, and per-part mass. D219's "zero loss" is not guaranteed today.
   - Clamp levels to the map's height range, keep them out of flood range, and respect the terraform rate limits.

## Q1. Does the plan hold end to end?
**Holds:** graph -> structure -> programs -> placement (X on hard facts, reasons) -> joint levels -> render -> signals.
- T2/T3/Z2e measured it: 18/48 terraform solved, ridges 228/228, T3-clean.
- The hover is wired: H1-H4, 19/48 planned, 3.7 ms mean.

**Broken:**
- (a) **Relation completeness under edits.** See F1/F2 in Q3. The R7 WIP's flat Shift test needs WARN plus `clean >= 1` of 2 sides (WT:2885, :2890) to pass. That is the checker finding missing relations, which D193 says is a model bug.
- (b) **State cost.** ApplyJunctionShifts calls CutJunctionTrackBridges (W:4144), which enumerates cuts, each a full placement. It also places without fixed choices, so vertical S-bend m is re-derived from columns (J:2884). §14.2 says m, the cut, the hug and the port offset are per-state variables changed only by edits. Re-deriving them is the "S-bend sizes grow with columns" growth, and most of the 60-70 ms (INFERRED).
- (c) **Terraform mode's X phase uses class facts only** (Z2c note). Columns are chosen blind to heights; levels then pay whatever those columns cost. Fine for feasibility, but the cost edit (D217) or §17 node spots must close it.
- (d) **§16.1 ("placement has no per-shape row code") is not met.** Per-row `in_edge[r]`/`out_edge[r]`/`leg_edge[r]`, `lane_item`/`north_item` and `doubled_row(r)` still drive the Y relations (J:2181-2186) and the doubled-leg block (J:2240-2262). The R7 WIP refuses any undoubled-leg token but S-bends ("not ruled yet", W:~1443). So node moves to another row and multi-leg rows are not yet possible. T4b's node-move edit depends on them.

**Change in order:**
1. F1 (legality) + R7 as in top-5 item 2.
2. The edit-soundness sweep.
3. Only then N1's prototype negotiation and T4b.
4. Height-model changes (Q4) before Z3 ramp tokens: ramp tokens on top of ADJ would bake in its lockstep restriction.

**Scope:** LL_RR hover in the GUI (T only today, RECON-hover-switch §2) belongs before any negotiation tuning. It is the topology where Shifts matter.

## Q2. §17 snap and negotiate
**Sound as a search frame:** a two-level search, with an inner best-first assignment plus nogoods inside the outer A* over structural edits (§14.6). It is close to conflict-based search (CBS) from multi-agent pathfinding. That is a known, sound frame *when each item's cost is independent given the constraints and the low level re-plans per branch*. As written, §17 meets neither condition.

**The premise is false.** §17.1 says "run lengths, bend positions and ramp lengths are already optimal from propagation". Propagation gives the least *feasible* solution, not the cheapest. In terraform mode X ignores heights entirely (Q1c). Only node spots are given a cost search (§17.2). Runs and bend positions also need the D217 cost edit (a point to the cheapest column in [Lo, Up]), or they must be part of the edge candidates.

**h is not admissible as claimed.** Per-tile corner moves "ignoring propagation" bound one tile from below, but L's Evaluate sums |target - current| over a tile's 4 corners (L:~93). Summed over tiles and items, a corner shared by up to 4 tiles is counted up to 4 times, while the real plan counts it once (terraform.cpp: `moves` per corner). So the sum over unassigned items' cheapest candidates can exceed the true cost.
- Fix: charge each corner move 1/4 per touching tile. Sum_tiles (1/4)|delta| <= max |delta| <= the true cost.
- Or use max over items (weak but safe).

**Item costs are not independent:**
1. **Rows.** R7's Shift inserts a global stack item (W:4129). Every item beyond it moves a row, so every traced candidate on that side goes stale. A Shift is therefore *structural* except on the stack's outer side, where no other line's rows change.
2. **Heights.** ADJ ties adjacent-row track corners (L:279). Two lanes on adjacent rows must ramp in lockstep, so two individually cheap candidates can be jointly expensive. "Heights are never a conflict" (§17.3) is false: Z2e still fails 1/18 (conflict or spill).
3. **Node spots move edge ends.** A node's spot is the end of every incident edge, so edge traces "between current ends" are stale as soon as a node is assigned.
   - Fix: assign node spots first, as variables.
   - Then edge shape = a link cost memoised per (edge, from spot, to spot).
   - h = the sum over unassigned edges of the min over compatible spot pairs.
   - On a conflict (tile overlap, or a rule clash named by the full solve), branch CBS-style: "item i avoids tile t" or "item j avoids tile t", re-tracing only that item. This handles staleness by construction instead of "re-trace if frequent" (§17.5).

**Where it will fail:**
- **Cross-edge rules invisible to a tile-overlap pre-check:**
  - curve gaps between curves of consecutive edges on one route (a Shift adds 2 curves per route through it, §16.4);
  - the waiting room (node to node, J:2190-2205);
  - prio coverage;
  - doubled-lane equal length (junction_rules.cpp:493-502);
  - bridge middle clearance.

  Each needs a full solve today. Add the curve-gap family on the route unit positions to the pre-check (cheap: the units are linear in columns).
- **The D215 objective moves points after the solve** (J:1254 loop), so where a candidate was traced is not where it ends up.
- **Budget.** A full solve is about 1 (placement) + 7-10 (levels) + 2 (render/signals) ms. N1's 20 full solves is 200+ ms (Q7).

**Shift: candidate or structural?** Both, by where it lands:
- On the outer side of the whole stack, the new row holds only the shifted stretch. Nothing else moves, so it is a local shape and a negotiation candidate.
- Between items it is RowGap + Shift, a structural edit in the outer A*, restarting at programs + placement (§14.6 as written).

## Q3. The R7 failure
**Likely real cause (INFERRED from code; confirm with the diagnostic below):** the Shift S-bend crosses other lines at grade.
- W:4120-4129 puts the Shift item "beyond the row's own side tracks" (copy, approach, prio row), not adjacent as the brief said (R7 brief, "The edit"). W:2365 relaxes the S-bend to `ge(1, ..., 1)` rows, so the S-bend spans every item between the edge's item and the Shift item.
- On a doubled row the in edge runs on the north (exit) item (J:2183-2185), and a south Shift crosses the lane row too.
- Any line on an intermediate row within the S-bend's columns is an at-grade crossing that the graph has no node for (D182: every crossing is a node).

**Why the checker shows these two messages:** "doubled lanes differ in length" is raised for *any* movement that traces more than one path with distinct middles and unequal lengths (junction_rules.cpp:461-503). A crossing gives a single-lane movement a second path. "Unexpected exit" (:281) is a trace leaving through the other line.

**Why disabling the tie did not fix it.** The occupancy family has two holes besides the tie:
- **F1, a pair-keyed whitelist.** `ref_shares` holds *edge pairs* (J:1999). Once e6 shares any tile with edge X in the reference, e6 may share *every* tile with X (J:2008). All node and prio occupants carry `edge = UINT32_MAX` (J:1918-1921), so one node sharing with X legalises every node sharing with X.
- **F2, invisible runs.** Runs that are empty at the flat solve are not occupants at all (J:1902-1906 "nothing when there are none at the flat solve"). Floors and facts then stretch them across the S-bend's column with no relation between them.
- Also: relations are only between *neighbouring* groups in the flat order, and none when any member pair is related (J:2046). A crossing line whose order is decided by a third, related occupant gets no constraint (INFERRED).
- Diagnostic: for e6/e20, list the S-bend's tiles per intermediate row and the occupants of those rows at the flat solve, and the first tile where the two traced paths of the violating movement differ.

**The right rule: not "root-seed reference".** That moves the same pair-keyed hole to the root.
- (i) Shared-tile legality is tile-local. The pieces are related by the graph, i.e. `related` (J:1988) completed with whatever makes e12/e17 at (28,11) legal: read it from the checker's trace (T1c2 Amendment 2 asked for this and it was never answered). Or no two unrelated pieces share a track end (the existing test, J:2010).
- (ii) The order and ties of unrelated occupants are per-state choices from the root, mapped through each edit. An edit's new elements get their order from the edit itself (a Shift's stretch lies between its S-bends on its own row).
- (iii) An S-bend may cross a row only where the row holds no track over the bend's columns. With global stack rows, that means the Shift item must be adjacent.

**Shift item beyond all row items: wrong.** Put it adjacent to the edge's item. That is a row gap at that boundary: side items move one row out and their S-bends/fans grow by one row through the relations. Beyond is right only when the edge's item is already outermost on that side.

**Separate defects in the WIP:**
- Prio refusal is per row (`leg && prio_row`, W:~4108), not the coverage stretch the brief names (within even_prio + 1 of the joiner merge).
- The flat test is tagged `[topologysweep]`, not the brief's `[topology]`.

## Q4. The height model
- **One level per track tile (entry level, L:~170-216): right and general for axis track.** Nodes, heads, curves and switch tiles are flat (rail_cmd.cpp GetRailFoundation). The prio tie (Z2f) is right given ADJ.
- **ADJ / FLEX / SLOPE2 are fitted, not derived.** RECON-joint-levels picked them by fixture count; SLOPE 3/4 "add nothing". All three approximate one game rule: axis-adjacent corners differ by <= 1, which is exactly what PropagateJunctionTerraform enforces afterwards (terraform.cpp:111-121).
  - Make corners of the box + margin variables in the same difference system: `|c_i - c_j| <= 1` for axis-adjacent corners, immovable corners fixed, track corners tied to tile levels.
  - Spill and SLOPE2 then come out of the system itself.
  - Cost = sum |c - ground| over corners: a separable convex cost on a difference system, which min-cost-flow duality solves exactly (INFERRED; it would replace the greedy descent that is 1-2% off, RECON-joint-levels 2).
  - Foundations are the only non-convex part (a per-tile choice).
- **ADJ forbids a ramp beside a level track at the low level.** The game allows it with inclined foundations: X on a one-corner or steep slope gives InclinedX (rail_cmd.cpp:354-360, :317). This is conservative, but it bars "fan up" beside a level lane (D218) unless a row separates them. The engineer should know.
- **A model/game mismatch that will bite.** Evaluate costs a lone axis track on a one-corner-raised slope as a *levelled* foundation at z_max (L:99-108: `along` only excludes two-corner inclines). The game builds InclinedX there (rail_cmd.cpp:354): the track climbs.
  - T3 (EdgeZ uses GetRailFoundation) should catch it after the fact (INFERRED).
  - Add a unit test, and refuse or recost that choice.
- **The ramp rule (L:219-230) has three gaps:**
  - (a) Windows containing a bridge middle are skipped, not counted. A route can climb k just before a near head and k just after the far head within one train length (tl >= 6 with a 2-tile middle). Count middle tiles as flat visits instead.
  - (b) Windows count tile *visits*, while the train length is in units. Diagonal half pieces make a tl-visit window shorter than the train (INFERRED: weaker than D219).
  - (c) k = tl x spare / slope (junction_generator.cpp:108-121) assumes uniform mass per tile. A loco-heavy consist on a short ramp feels more than its share (RECON-slope-physics §4 flags it as INFERRED). "Zero loss" needs the heaviest k-tile stretch of the consist, from the picker's per-part weights.
- **The game's terraform vs the plan (all INFERRED, none tested in game yet):**
  - Level range `lowest - 3 .. highest + 3` (L:171) has no clamp to >= 0 or to the map's height limit, and no guard against level 0 next to water (flooding).
  - The company terraform and clear rate limits (`terraform_frame_burst`, `clear_frame_burst`) can stop a large plan mid-build; nothing in junction code reads them.
  - The game's per-command propagation is order-dependent, so its cost can differ from the plan's. That is Pete's H2 checklist item; add the SimulateTerraformSteps test RECON-hover-switch §4 proposed, if H1 did not.
  - Terraform is posted *before* rail (H2), and it is not reversible.

## Q5. Rulings made while the engineer was away: wrong or risky
1. **T1c2 Amendment 2 (legality from the unfloored reference): wrong.** Pair-keyed and solution-derived (F1/F2); now the R7 failure.
2. **T1d Amendment 1 (keep the out-port and lane-split relations exact, "relaxing crossed rows"): risky.** The crossing was the occupancy hole, not a reason for exactness. It re-pins two of the preference pins FABLE-terrain-placement item 9 asked to justify.
3. **T1d Amendment 2: risky debts.** Pins moved to measured values with "checker-clean sweeps are the proof", but the sweeps cover seeds and stretches, not edits. P4b tests now WARN and return. The :1794 check is weakened. Tile naming is relaxed at :3077 and 49,5. Keep all four on the debt list.
4. **H2 default ON + terraform first + no checker in acceptance: risky.** It contradicts D193's "until RC0, the search keeps the checker" now that edited states are coming. Default ON before Pete's in-game check is also premature.
5. **Z2d/Z2e: adopting ADJ/FLEX/SLOPE2 by fixture count: acceptable as a stopgap,** but it is a patch (Q4).
6. **§17.1's "lengths already optimal": wrong** (Q2).
7. **R7 WIP: should not be merged as it stands.** It moved the Shift item beyond the side tracks and turned failures into WARN / `clean >= 1` / a loosened box bound. The brief said adjacent and clean.
8. **Fine:** T1c1 Amendment 1 (k_out as a lower bound; the D215 objective restores it). R1 (seeds as the oracle). H3 Amendment 1 (prio element levelled; floors back to 18/19 after Z2f, real_terrain.cpp:251/:327).

## Q6. Site rules in disguise and test debts
**Site rules in disguise:**
- Per-row in/leg/out arrays and `doubled_row` (Q1d).
- The prio refusal per row (Q3).
- `ref_shares`: a per-site whitelist computed from a solution.
- Solution-derived sides re-derived per state: hug (`hug_heads`), bridge clearance side (plan_clearance J:2803-2848), occupant order. They are hidden choices of exactly the kind §14.2 made explicit for m and the port offset.
- SLOPE_RANGE = 2 and the ±3 level range: measured constants, not derived.

**Test debts:**
- The R7 WIP test (above).
- T1d's four relaxations (Q5.3); [topoproj] Debug over budget (5.13 s).
- Z2b "no reds recorded".
- RECON-joint-levels 4.2: 4 fixtures whose terraform-mode placement differs from the hard placement (width 41 vs 50/52/43/42), not investigated.
- R5's copy-1 hand taken from `program.back()`, unchecked.
- D219's realistic-model guard only in the junction GUI (console export and the station GUI are unguarded, Z2b).

**Missing reds:**
- The edit-soundness sweep.
- A one-corner-slope Evaluate test.
- A ramp window across a bridge.
- Level clamp / flood.
- Game-vs-plan terraform.

## Q7. The < 100 ms hover
**Where the time goes per full solve (measured figures from STATUS / RECON-joint-levels):**
- placement ~0.2-1 ms;
- levels 7.4 ms mean, 19.5 ms max (SLOPE2 is most of it);
- render + signals ~2 ms;
- any structural edit through ApplyJunctionShifts: 60-70 ms.

So ~8 full solves fit, not 20.

**Must be incremental:**
1. Edits keep the parent's cut and choices: no CutJunctionTrackBridges, no rounds. Placement only, ~1 ms.
2. Levels warm-started from the parent. Re-descend only the tiles on changed routes plus their SLOPE/ADJ neighbours. Index SLOPE2 by corner (RECON-joint-levels 5).
3. Negotiate on X + hard facts + a per-tile level lower bound. Run the joint levels only on the best X-feasible assignment.
4. The D215 objective (J:1254-1290) is O(changes x points x propagation) per round, since each push restricts every other point and undoes. Compute a group's push bound from its incident constraints instead.
5. Render / signals / checker only for the accepted state.
6. Warm start across cursor moves (FABLE-projection item 8).
7. Measure the main-thread snapshot: 80x40 QueryJunctionClear test-runs, unmeasured (RECON-hover-switch §5).

## Q8. Decisions for the engineer
1. The occupancy legality rule: tile-local and graph-related, no reference (Q3).
2. Shift: adjacent item; local candidate on the outer side, structural elsewhere (Q2/Q3).
3. The checker in hover acceptance until the edit sweep is green; hover default ON or OFF until your in-game check.
4. Heights: corner variables (exact) vs the ADJ patches; whether a ramp may sit beside a level track via inclined foundations.
5. The ramp rule: per-part mass; bridge windows; units.
6. Terraform safety: height clamps and flood, rate limits, terraform before rail (irreversible).
7. Workstreams: STATUS line 2 queues nothing for the line painter or stations.

## Suggested tests
- `[topology]` edit soundness: 50 random edit lists (Shift either side, row gap +1, m +/- 1) on T and LL_RR flat, each checker-clean or refused with a reason. No WARN.
- `[topology]` Shift on LL_RR e6/e20: S-bend tiles never share a tile with an unrelated edge; no extra traced path.
- `[topology]` Evaluate: a lone X piece on SLOPE_N at z_max is never a levelled foundation.
- `[topology]` ramp rule across a 2-middle bridge at tl 8, k 1: a climb on both sides within tl is refused.
- `[topology]` levels never below 0 or above the height limit.
