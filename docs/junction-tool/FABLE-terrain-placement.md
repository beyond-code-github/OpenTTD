# FABLE-terrain-placement: review of DESIGN-G1 §14 and of Pete's steers D213-D216 (at ec9d362548)

Read-only review. Read: DECISIONS D182-D216; DESIGN-G1 §11-§14; DESIGN-nodes; DESIGN-N3 §6-§11; FABLE-nodes-perf; FABLE-projection; RECON-seed-floors, -detour, -graph-input; STATUS line 2 and the P4b/SF1b commit notes. Code: src/junction_topology.cpp PlaceJunctionTrackGraph (:902-1560), CutJunctionTrackBridges (:2310); src/junction_positions.{h,cpp}; src/junction_facts.h. Line numbers are junction_topology.cpp unless a file is named. INFERRED = reasoned, not run.

## Verdicts

**§14: sound in direction, with changes.** Solving the graph's placement relations on the terrain is the right model. Placement is also about 10x cheaper per state than the node path: the cut search places LL_RR 6 times in under 1 ms (§11.9b), against about 8 ms per state on the node path (P4b: 2 states, 17 ms). §14 has four gaps:
- (a) today's placement is not a pure difference system. It has three hidden choices (below).
- (b) it lacks the occupancy and general curve-gap relations that the node template had. On flat ground its output was clean only by construction.
- (c) it treats D215 as an exact relation. That blocks the most common obstacle.
- (d) it does not say that every structure edit needs placement to accept row shapes it refuses today (:738, :970).

**Pete's steers:**

| Steer | Verdict |
|---|---|
| 1. D213, projection depends on the graph | **Holds.** |
| 2. D215, row change next to its feature | **Holds as a preference. Better as an objective than as an exact relation (Q1.2).** |
| 3. D216, the graph is the seed, no flat stage | **Holds with a change.** No flat *positions* are carried over. A flat *solve* is still worth keeping: it gives the root's discrete choices, the cost bound and the oracle. |
| 4. Edits for elevation and obstacles | **Holds.** It is also the only way: an obstacle on a continuous row can never be absorbed by positions (Q2). |
| 5. Nothing slides, stretches or leans unless it is in the graph | **Holds and is workable.** Placement points already are graph variables. Three hidden choices must become explicit graph variables. |

## Q1. The steers

**1. D213.** This is right. A state is (graph, edit list) and the solve reads only graph relations. Formulation: "an obstacle mutates the graph by an edit; the state is re-placed".

**2. D215.** On flat ground it costs nothing: SF1b made e8's lane split exact and the box stayed 62x25 (RECON-seed-floors §2, §4). As an *exact* relation (§14.3, §14.4) it removes exactly the slack that terrain needs:
- Take a house on the port row between a W port and its in-fan. To avoid it, the fan must start before the house, so fif < c. With `nex - fil = d_min` exact, fif is tied to the exit split. The avoid rule (junction_positions.cpp:69) then raises lo[pin], and the port is fixed (`eq(port,0,0)`). The state is infeasible and needs an edit, although moving the fan toward the port would do.
- The exact lane split `:1304` and the head-hugging `:1355` make the whole doubled leg one rigid group, so a house on the lane split tile moves the bridge.
- Exact chains have bitten before: SB1b's exact split-to-heads chain gave positive cycles on every LL_RR cut (§11.9b).

*Better:* make D215 a stated objective on named runs, not an equality.
1. Solve the least solution.
2. Fix every point except the D215 tokens, including xe and each token's feature node.
3. Push each D215 token to its extreme toward its feature (JunctionPositions Up or Lo, then propagate), in a fixed rule order (outermost lane first; FABLE-projection Q3c).

On flat ground this equals the exact relation (same numbers as SF1b). On terrain it gives way by itself, with no edit. This is P3's lean, but on a graph variable whose preference the graph states. The P3b blocker (a lean class shared with bridge heads) does not exist here, because each token has its own point. So it meets steer 5's "unless that's in the graph". Pete's reason ("prevents everything upstream from becoming more compact") is kept.

**3. D216.** "The graph is relations, the seed is their solution on flat facts" is right, and so is "pinning too many tiles" for the template (rigid classes at heads, P3b). A flat solve per (spec, topology) is still useful, as D184 already memoises:
- (a) the root state's *discrete* choices: bridge cut (CutJunctionTrackBridges enumerates placements, so it must not run per state), port-row offset, vertical S-bend sizes, rows;
- (b) an admissible cost bound, because facts only add constraints and so only raise the least solution;
- (c) T1's oracle;
- (d) the Y phase (Q2(d)).

Wording to adopt: "no flat positions enter the terrain solve; the flat solve supplies the root structure and the bound". Placement itself also pins things that are preferences, not shapes: `:1304` (lane split exact), `:1355` (heads hug crossers), `:1380` (k_out exact), `:1382` (port stub exact). Each needs a stated reason, or it becomes an objective or a variable.

**4. Elevation and obstacle edits.** Yes. See Q3 for the list.

**5. Only graph variables move.** Workable, because every placement point is a graph element: a run length is the difference of two points. The steer then means "no post-solve pass moves anything except a declared objective" (Q1.2). Three things in today's placement move outside the relations, and must become explicit graph variables, fixed per state and changed only by an edit:
- **Vertical S-bend size `m`:** set from the X solution, then iterated (`:1477`, `:1515-1526`, "does not settle").
- **Port-row offset:** a minimax outside the system (`:1487`).
- **The joiner_div corner size:** taken from the rows (`:1399`).

The full list of graph variables: run lengths (as point differences), row gaps between stack items (`:1204`), band depths, S-bend `m`, fan `m` / port-row offset, bridge cut, bridge span extension (Q2(c)), and later the cursor offset (D197). Rigid shapes (bend halves, switch tile, prio element, head stagger) are not variables.

## Q2. Are the §14.2 relations monotone? (14.6 a-d)

- **Runs (a):** "no bad tile in [a,b] on a fixed row" is min-closed and max-closed: [min a, min b] lies inside a solution's interval. StraightRun / run_id equality for heights is the same interval property (junction_facts.h StraightRun).
  - **But on a continuous row the avoid cascades.** Pushing run start a past c leaves c in the previous run on the same row, and so on back to the last row change. An obstacle in a row's through-run is therefore absorbed only where the row's coverage ends (a fan, a lane split, a bend, a node) or by a vertical column moving.
  - So T2 will solve only tiles near row changes, nodes and columns. Say so up front, so that T2 is not judged a failure. A house mid-row always needs an edit (it is P4b's finding again).
- **Foundations (a):** monotone wherever a tile's track bits come from one element. Each straight tile is one axis and each rigid shape has a fixed mask.
  - **Not monotone where two elements share a tile:** parallel diagonals (D142) and switch tiles of two edges. Fits and EdgeZ take the whole tile's bits (junction_facts.h Fits/EdgeZ), which couples two variables.
  - Fix: put the sharing elements in one rigid group (bundle, FABLE-nodes-perf §1.3), or require the shared tile to be flat as a conservative unary domain.
- **Bends on slopes (b):** a per-anchor domain is exact *if the bend's size is fixed*. Corners have k fixed. Vertical S-bends (`m` from columns) and joiner_div corners (size from rows) sweep a footprint that depends on two variables, which FABLE-nodes-perf §1.2 warned against. Fix their size per state (Q1.5). The domain is then a mask AND the fact rows.
- **Bridges (c):** z(near) = z(far) on a pair with a variable span is **not** min-closed. Counter-example: z = 0,1,1,1,1,0 gives solutions (0,5) and (1,2), but min (0,2) fails.
  - Make bridge, crossers and heads one rigid group: crossers packed exactly. Packing is today only a checked postcondition (§11.8 F6), so make it a relation. The group's mask then also covers head slopes, clearance over the crossers and the middle tiles: one unary domain on its anchor.
  - A longer span is an edit "extend span of bridge b by 1 at near/far", up to max_bridge_middle. It is not a pair domain.
- **Y then X (d):** every fact needs both coordinates (a row run needs its x range, a column needs its x). So the Y phase cannot use facts monotonically, and the greedy fact-aware Y snap (DESIGN-N3 §10) is the hidden search FABLE-projection Q6 warned about.
  - *Better:* the Y phase takes **no facts**. Rows equal the flat rows unless an edit changes a row gap or a band depth. The X phase sees every fact with Y fixed, which is exactly monotone. A failure whose chain ends on a row run or a column's rows names the stack boundary, and the edit is "row gap +1 there" or a Shift.
  - That is cheap (Q6) and deterministic, and it removes the fact-blindness question instead of measuring it.
- **Missing relations (not monotonicity, but correctness):** placement has no general occupancy. `:984` leaves band slices and vertical edges against side tracks to the render tests. It also has curve gaps only at the fans (`:1275-1295`, `:1368-1380`); SF1b's tl >= 5 violations at (25,7), (44,15) and (27,13) are this gap.
  - The node template had both: DESIGN-N3 §7 chain and rule 3 pairwise.
  - On terrain, runs stretch unlike flat ground, so "clean by construction" no longer holds. Add per-row and per-column order relations (from order.rows / band layers, gap 1 or 2 by edge contact as OC1). Add pairwise curve gaps per path as late X differences (units along the path are linear once Y is fixed, as NP2 did).

## Q3. §14.4 relations vs edits

The split is right. Each edit, the failure that produces it, and where it restarts:

| Edit | Produced by | Restart at |
|---|---|---|
| Row gap +1 at stack boundary i | X failure on a row run or column, named row | placement |
| Shift (D182/D195, §13.9.1) | mid-row obstacle (cascade ending at a port or node) | programs + placement |
| Span extension | bridge-group domain empty, named bridge | placement |
| Re-cut bridge b | as span extension, when the cut is the cause | structure |
| S-bend m +1 | domain empty on that bend | placement |
| Ramp (RS1, D196) | Height on a bend or diagonal | programs |
| Crossing swap (D190) | roots, not children | structure |
| Node move | footprint domain of a node empty in its whole range | order |
| Terraform | fact overlay (D196); no graph change | X solve only |

- Every failure is the propagation's explanation chain. JunctionPositions keeps no reasons today (junction_positions.h has no reason API), and placement only says "contradict each other" (`:1100`). T1 must add a reason per lo/up change on the trail.
- **Cost per state (INFERRED from the measurements above):**
  - Y solve plus X solve with facts: under 0.5 ms for LL_RR.
  - Render and signals (about 2 ms, RECON-seed-floors §4) only for the accepted state.
  - That gives roughly 100+ states in 100 ms, against about 12 on the node path.
- **The undesigned part (§13.9.3, not mentioned in §14):** placement only knows rows of shape in + one leg + out (`:738`, `:970`) and fixed token kinds per edge. Shift and node-move edits need a general row chain: any sequence of tokens and nodes on a row, with floors per token kind. That is the largest piece of work in T4, so design it before T4 is briefed. RECON-detour's "insert SBend tokens into programs" recommendation is the same mechanism.

## Q4. Vehicle, node path, oracles

- **Vehicle:** placement relations on JunctionPositions (one instance per axis, the trail kept for objectives and explanations). The alternative, a node solve generalised from graph relations, would rebuild what placement already is, minus its 10x speed. Port from the node path:
  - JunctionFacts;
  - HF1's join rule, as mask contents;
  - OC1's occupancy chain idea, as static order relations;
  - pairwise curve gaps;
  - P4a's element naming.
- **What happens to P1-P4b:**
  - P2 (signal tokens from graph and routes) is reused as is, after placement.
  - P4b's best-first frame (edit lists, cost, children validated) is reused, with placement in place of the template solve.
  - The P4b detour becomes a program edit.
  - P3's lean retires into the Q1.2 objective.
  - P1 (graph input, tags) stays only as an oracle: the template built from the placement's output must build.
- **Oracles (§0b):**
  1. T1: placements equal today's for T and LL_RR, with flat facts.
  2. **A solution-space soundness sweep:** add random lower bounds on run lengths, row gaps and m (a forced stretch), solve, render, then run CheckJunctionRules, JunctionRoutesHeightsOk and fits. Every feasible stretched state must be clean. This is the D193 test that relations are complete. Today's seed-only tests cannot show it, because the least solution hides every missing relation.
  3. The N3c house, hub-tile and incline cases and the [nodeoracle] counts gate the hover switch.

## Q5. Steps

Proposed order:
- **T0 = TL1:** placement floors scale with tl and speed; checker clean over the sweep.
- **T1:** JunctionPositions, explanations, and m / port offset / joiner corner as explicit per-state variables; identical seeds.
- **T1b:** occupancy and pairwise curve-gap relations, D215 as an objective, crossers packed exactly. Outer: the stretch sweep (Q4).
- **T2:** blocked tiles as avoids and masks. Outer: P4a's sweep, with the expectation stated from Q2(a).
- **T3:** heights and bridge-group masks. Outer: the incline and ridge cases.
- **T4a:** general row chain in placement (design first).
- **T4b:** edits from explanation chains in the P4b frame. Outer: core house by row edit or Shift, states gated.
- **T5:** hover, warm start (edits position-free, FABLE-projection item 8), §0b gate. Outer: p99 < 100 ms.
- **T6:** terraform and tunnel.

What is missing from §14.5: T0, T1b, T4a, the stretch sweep, and a timing line at T2 (one LL_RR state with facts, Release).

## Q6. Risks

- **Hover:** fine per state (Q3). The risks are the number of edits on rough land (rows times obstacles) and the cut search if it is left inside states. Fix: the cut comes from the root, and re-cut is an edit.
- **"Does not settle" (`:1526`):** this will get worse on terrain if m keeps iterating (Q1.5).
- **TL scaling:** the placement constants at `:1275-1382` were proven only at tl 4. Terrain failures will mix with rule failures until T0.
- **Shared-tile facts and bridge z:** non-monotone (Q2). Use rigid groups.
- **Asymmetry:** the W ports are pinned at x = 0 and the least solution pushes east, so an obstacle near the W edge grows the box east. That is acceptable under D197 (the cursor spot comes first). Note it in a test.

## Changes to §14 (numbered)
1. §14.3: D215 is an objective on named runs (after the least solution: fix the others, push each token toward its feature, in rule order), not an exact relation. Replace "every run at its rule floor" with "the least solution, then the D215 objectives".
2. §14.1/§14.4: list the graph variables (Q1.5). S-bend m, the port-row offset and the joiner_div size become per-state variables, and the fixed-point loop and the minimax go.
3. §14.2(d): the Y phase takes no facts. Rows change only by edits named by X failures.
4. §14.2: crossers under one bridge are packed exactly, making a rigid group with one mask. A span change is an edit. Shared-tile elements are bundled or need flat tiles.
5. §14.2: add occupancy order relations and pairwise curve gaps as late X differences. State that a mid-row obstacle is never absorbed by positions.
6. §14.4: the edit table (Q3), with each edit's trigger and restart stage. JunctionPositions gains reasons on its trail for explanation chains.
7. §14.5: steps T0, T1, T1b, T2-T3, T4a (general row chain, designed first), T4b, T5, T6. Add the stretch-sweep oracle and the T2 timing.
8. D216 wording: "no flat positions; the flat solve gives the root structure (cut, rows, sizes), the cost bound and the oracle".
9. Release-preference pins in placement (`:1304`, `:1355`, `:1380`, `:1382`) are each either a rule shape with its source, or an objective or variable.
