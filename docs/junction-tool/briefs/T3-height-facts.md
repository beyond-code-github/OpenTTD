# T3: height facts in the X phase: run joins, element fits and joins (DESIGN-G1 §14.3-§14.4, §14.7 T3; D194, D216)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git checkout -B wt-t3 <HEAD>` (given at launch). Never use git stash.
- Merged T2: the facts overload of PlaceJunctionTrackGraph applies, in the X phase, group domains (every SymTile of an element: Half for bend tiles, Switch for node tiles, Straight along travel for heads and vertical-run tiles) and AddAvoid per bad Straight(X) column over each row run. Those per-tile facts know a tile's slope fit, but not whether neighbouring pieces meet at the same height. The node path checks that after its solve (JunctionNodeHeightsOk; JunctionRoutesHeightsOk over routes); placement must have it in its relations so that a solved placement is always buildable (D193: correct by construction).
- Rows (Y) stay fixed; a ridge across rows that no column choice avoids fails with a reason (the row edits come in T4).

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); TESTS.md (`[positions]`, `[topology]`, `[nodeoracle]`, `[topoprojsweep]` rows); DESIGN-G1.md §14.3-§14.4.
- src/junction_facts.{h,cpp} (Straight, Fits, EdgeZ, StraightRun, run_id, NextBad); src/junction_positions.{h,cpp} (AddAvoid, its propagation and causes).
- src/junction_nodes.cpp JunctionNodeHeightsOk and JunctionRoutesHeightsOk (what they check: per-piece fit and join heights).
- src/junction_topology.cpp the facts block T2 added, edge_tiles, bend_tiles, and where the render gets each element's piece bits.
- src/tests/junction_game.cpp "the node search's layouts pass the rule checker on random terrain" (`[nodeoracle]`, its 250 terrains and offsets); src/tests/junction_nodes.cpp "the node solve moves the W end off an incline" (:297).

## Tests (record each red)
1. **Outer, new tag `[placeoracle]` "placement on facts is buildable on random terrain"**: the `[nodeoracle]` test's 250 terrains (same seeds, same generators, same skip rule), with the T's graph seed (MakeJunctionGraphSeed(JunctionTTopology(), spec)) placed by the facts overload with its own choices, at the offset that puts the graph seed's hub where the node test puts the traced T's hub (derive it; state it). Per terrain where the placement solves: render, JunctionRoutesHeightsOk (heights), CheckJunctionRules with the T topology: count `heights` failures and violations. REQUIRE heights == 0 and no violation (INFO seed and each). Print `PLACEORACLE terrains= skipped= solved= heights= violations= micros=`. Red: before the changes, some solved placements fail heights (record the count).
2. **`[topoproj]` "topoproj: placement moves the W end off an incline"**: the terrain of :297 with the T graph seed at the same offset rule: the placement solves, heights ok, checker clean. If the placement cannot solve it with rows fixed, record its reason chain and make the test CHECK the failure names the incline (a fact reason) instead; say which.
3. **`[positions]` "positions: an avoid between two columns"**: AddAvoid(a, b, 4, 5) forbids exactly x_a <= 4 and x_b >= 5: three small systems (forced left, forced right, infeasible) with Lo/Up and Explain.
4. **`[facts]` (or the tag facts tests use) "facts: Joins"**: on a terrain with a step between two flat tiles on a row: Joins false at the step, true elsewhere.
- Every existing test stays green unedited; `[topoprojsweep]` (T2's 20 houses: still 12 solved, all clean) unchanged.

## Changes
1. **JunctionFacts:** `bool Joins(Axis axis, int line, int pos) const; ///< Whether straight pieces at pos and pos + 1 on the line meet at the same height (one straight run).` from run_id.
2. **JunctionPositions:** `bool AddAvoid(size_t a, size_t b, int c_a, int c_b, uint32_t reason = NO_REASON); ///< Not (x_a <= c_a and x_b >= c_b).` The existing AddAvoid(a, b, c, reason) becomes this with c_a = c_b = c. Propagation as today generalised (lo_b >= c_b forces lo_a > c_a; up_a <= c_a forces up_b < c_b).
3. **The facts block (X phase):**
   - **Row runs:** for each boundary c on the run's row where both tiles are straight-ok but !Joins(X, row, c): AddAvoid(lower point, higher point, c - offset.x, c + 1 - offset.x), reason "height step".
   - **Vertical runs:** the anchor's domain excludes columns where any consecutive pair of the run's tiles does not join (Joins(Y, column, y)).
   - **Elements:** the anchor's domain keeps a column only if every piece of the element Fits with its own bits there and every join inside the element, and each join to the tile just outside it on either side along its travel (the run's first or last tile), has equal EdgeZ on both sides (the same check as JunctionRoutesHeightsOk, per element). Reason "height join". The bits come from where the render gets them; if an element's piece bits are not available without rendering a layout, STOP and report where they come from.
   - **Shared tiles:** a tile holding pieces of two elements whose anchors are not tied by an exact relation: report each such tile (edges, kinds) in the report; do not add a relation for it in this step. If one makes test 1 fail, STOP.
4. TESTS.md: a `[placeoracle]` row (gate tag; Release time; budget 10 s), `[positions]`, `[topoproj]`, facts rows.

## Do not
- No row edits, ramps, terraform (T4, T6). No change to the node path, its tests or `[nodeoracle]`. No change to relations outside the facts block.

## Stop conditions
- Test 1 still has heights failures or violations after the changes (report each distinct one: seed, tile, edges, which join); element bits unavailable; an existing test fails; anything not covered.

## Gates
- `tools/junction/jt fast build '[positions]' 5`; `tools/junction/jt fast build '[topology]' 5`; `tools/junction/jt fast build '[topoproj]' 5`; `build-rel/openttd_test "[placeoracle]"` and `"[topoprojsweep]"` (to files); `tools/junction/jt gate build-rel` (422 before; report the count).
- One commit: `Add: Junction placement height facts: run joins, element fits and joins`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus the PLACEORACLE line before and after and the `[nodeoracle]` ORACLE line for comparison; the offset; test 2's outcome; the shared tiles found; mean Release micros per placement. When you have reported, stop.
