# T1c-2: the occupancy relation family replaces C1/C3; bridge heads hug by order, not a pick (D217; DESIGN-G1 §14.3; RECON-stretch C1-C3)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git checkout -B wt-t1c2 <HEAD>` (given at launch). Never use git stash.
- D217 (read it): relation families everywhere, never per site. Today occupancy (unrelated tracks never share or touch a tile) is two site relations from T1b-1: "in fan order" (C1) and "bridge head clear of rows" (C3, north-only). RECON-stretch C2 (fans vs other rows after a row stretch) is not covered at all. The doubled bridges' heads hug crossers by `choices.hug`, a pick that goes stale after a row stretch (RECON-stretch §bridges).
- T1c-1 (merged) gave `y_floors` (row stretch seam) and a Y-stretch part of `[topologystretch]` that requires CurveSpeed clean and counts the other rules.
- T2/T3 enumerate every symbolic tile of a placement (element SymTiles, row runs between points, copy-1 tiles, node tiles) with piece bits (from RasterizeJunctionRoute): the facts block. This step reuses that enumeration.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); TESTS.md; DECISIONS.md D193, D217; RECON-stretch.md; AUDIT-rule-coverage.md (the "edge-contact occupancy rule" row).
- src/junction_topology.cpp the placement helper: the facts block's tile enumeration and piece bits, C1 ("in fan order"), C3 ("bridge head clear of rows"), the doubled-row crosser block and `choices.hug`, the fixed branch, `solve`.
- src/junction_nodes.cpp AddJunctionNodeOccupancy (the node path's occupancy rule, D193) for what "touch" means there.
- src/tests/junction_topology.cpp `[topologystretch]`, `[topologysweep]`, SamePlacement.

## The rule (settled)
Items: per row, every symbolic occupant as a column interval [left, right] given by (point, dx) at each end: a single tile (an element tile, a node tile, a head) or a row run between two points; per column, every vertical run likewise over rows. Two items are related if they belong to the same edge and copy, or one is a node tile of a node the other's edge starts or ends at, or one is a bridge middle over the other (middles are not items). For every pair of UNRELATED items on one row that are neighbours in the order of the state's flat solve (the fixed-choices solve with no facts and no floors; computed once per call, before the facts), add `left(right item) - right(left item) >= g`, where g = 1 when their facing pieces have no track end on the shared tile edge, else 2 (from the piece bits); if they share a tile at the flat solve (as LL_RR's fan pairs (2,4)/(3,3), (2,8)/(3,9), e12/e17 (27,10)-(29,12), e24/e25 (59,4)), tie them exactly instead (`eq` with the flat solve's offset, "shared tile"). Reason "occupancy" with both edges in the error text, the right item's edge in the reason.

## Tests (record each red)
1. **Outer, `[topologystretch]`**: the Y-stretch part now REQUIRES every rule clean in every solved run (not only CurveSpeed); the X part unchanged. Red: C2's occupancy violations under row stretches (record the counts per rule from T1c-1's report baseline, re-measured).
2. **`[topology]` "topology: occupancy replaces the site relations"**: with C1 and C3 removed and `choices.hug` removed, T and LL_RR at the test spec equal their seeds (SamePlacement); all 44 `[topologysweep]` cells clean and equal to before.
3. **`[topology]` "topology: doubled bridge heads hug their outermost crossers"**: on LL_RR and the T, under every Y stretch of test 1 that solves, each doubled bridge's copy-0 heads are exactly one tile outside the outermost crosser tiles on both of its rows. Red: the stale hug (RECON-stretch: T test, stack rows idx 0, s 1, bridge 0).
- `[topoprojsweep]` (12 of 20), `[placeoracle]` (heights 0, violations 0, solved >= its last count), `[realterrainsweep]` if merged: green; report solved counts.

## Changes
1. The occupancy family as the rule above, in the X phase after the relations and before the facts.
2. Remove C1 ("in fan order") and C3 ("bridge head clear of rows").
3. Bridge heads: remove `JunctionPlacementChoices::hug`; the crosser block's low/high tiles are the first and last crosser items in the flat solve's order on each of the bridge's two rows (the same order the family uses), and the heads are `eq` one tile outside them as today. (A bridge is then one group: heads tied to its outermost crossers, crossers ordered by the family.)
4. TESTS.md rows.

## Do not
- No new facts, no Z, no edits. No change to the checker or the node path.

## Stop conditions
- A pair the rule calls unrelated that the checker accepts sharing at the flat solve but that is not a shared-tile tie (report it); any seed change; test 1 still violating after the change (report each distinct: stretch, rule, tiles, edges — a finding, do not patch per site); an existing test fails; anything not covered.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `tools/junction/jt fast build '[topoproj]' 5`; `build-rel/openttd_test` with `"[topologysweep]"`, `"[topologystretch]"`, `"[topoprojsweep]"`, `"[placeoracle]"` and `"[realterrainsweep]"` if present (to files); `tools/junction/jt gate build-rel` (report the count). Circuit file byte-identical.
- One commit: `Change: Junction occupancy relations on every row and column replace the site relations; bridge heads hug by order (D217)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus: relations added at the test spec (LL_RR); the Y-stretch rule counts before/after; the shared-tile ties found; solved counts of the sweeps; [topology] Debug time. When you have reported, stop.

## Amendment 1 (orchestrator, after the stop; engineer away, orchestrator rulings, reversible)
1. **Groups (confirmed):** occupants on one tile at the flat solve form a group tied exactly; groups are the neighbours; related if any member pair is related (D193's chain-by-group). BUT a group is legal only if no two UNRELATED pieces in it share a track end on a tile edge (from the piece bits). The base seeds' groups must all be legal (if a base seed's group fails this test while the checker accepts the seed, STOP and report the tile, edges and bits). An illegal group (as e6/e16 on row 7 after a row stretch) is a placement failure with reason "occupancy" naming both edges, not a tie: the row-gap edit that made it is then rejected, which is correct.
2. **Bridge middle clearance family (replaces C3, both phases):** for every bridge B and every head H of another bridge whose flat-solve tile lies in B's middle line (same row for an X-axis B, same column for a Y-axis B) within B's middle span at the flat solve, keep H on its flat-solve side of B's line by at least one: in the phase of that axis (Y for an X-axis B: H's row vs B's row; X for a Y-axis B: H's column vs B's column), reason "bridge middle clearance" (B's edge, H's edge). The flat solve (relations only) is computed before the Y phase for this. This generalises C3 (which assumed the leg north of the head).
3. **Prio under a row stretch (RECON-stretch C5, rule 9):** out of this step. Test 1 REQUIREs every rule clean except rule 9, which it counts and INFOs; C5 is queued as its own recon.
4. **Time:** move the new "doubled bridge heads hug their outermost crossers" test to `[topologystretch]` (it runs Y stretches). If `[topology]` Debug is still over 5 s, report the per-test times of the five slowest `[topology]` tests and stop before committing.
5. Measure the relations added at the test spec (LL_RR) and check the circuit file (byte-identical expected; if it changes, commit it and say why).
