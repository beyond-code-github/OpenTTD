# Z2c: the height solve: a level per tile on every edge by a shortest path, producing a terraform plan and its cost (DESIGN-G1 §15.1-§15.3; D218, D219)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git checkout -B wt-z2c <HEAD>` (given at launch). Never use git stash.
- Merged: Z2a (JunctionTerraform: corner targets, PropagateJunctionTerraform, ApplyJunctionTerraform, cost), Z2b (JunctionRampLevels, hard-facts mode `heights = false`), RT1 (48 real-terrain fixtures, `[realterrainsweep]`), T3 (height facts: the check a terraformed terrain must pass).
- §15.3: after Y and X on hard facts, choose heights per route by a shortest path over (tile, level); heights never fail, they cost. First cut of the coupling (measure before anything richer): node tiles and port tiles at a fixed level each; each edge (and copy) solved alone between its fixed ends; a bridge's crossers under its middle at most the bridge's head level.

## The model (settled)
- **Units:** per edge and copy, its pieces in travel order with their tiles and bits (as T3 gets them from RasterizeJunctionRoute; bridge middles excluded; heads included).
- **Level and shape per tile:** a tile's track sits at level L; an axis piece along its own axis may instead be a ramp tile rising or falling one level over the tile (entry L, exit L ± 1); every other piece (diagonal halves, switch tiles, bridge heads) is flat at L (heads: flat at L, or the incline the game allows a head, D152b; keep heads flat in this step).
- **Continuity:** each tile's entry level equals the previous tile's exit level.
- **Ramp rule (D219):** with k = JunctionRampLevels(spec): if k < tl, consecutive up-ramp tiles (rising in travel direction) at least d tiles apart, d = the least integer with floor((tl - 1) / d) + 1 <= k (stricter than the window rule, simple; say so in a comment); down-ramps free.
- **Ends:** a node tile's level and a port tile's level are the cheapest flat level of that tile alone (ties: the lower); every edge starting or ending there uses it.
- **Bridges:** each crosser tile under a bridge middle has level <= the carried edge's head level (heads are node-like ends: fixed at their cheapest flat level first, before the crossers' edges are solved).
- **Cost of a tile at a shape:** if the ground already fits the shape (flat at L, leveled foundation at L with build_on_slopes, or an incline matching a ramp): clear_cost (+ foundation_cost for a foundation); else the corner moves to make it fit: |target - current| per corner x terraform_cost + clear_cost; a tile whose corners would move and whose class is not Buildable is not allowed at that shape. Levels considered: from (lowest ground corner of the edge's tiles) - 2 to (highest) + 2.
- **Search:** per edge, a shortest path over states (tile index, exit level, tiles since the last up-ramp capped at d) by DP in travel order; output the levels and shapes.
- **Plan:** the targets of every tile whose shape needs corner moves, merged over all edges (a corner targeted twice with different heights: report the tile and both edges, and fail the solve with reason "terraform conflict"); PropagateJunctionTerraform; its failure is a solve failure with its reason. Total cost = the plan's cost + per-tile foundation and clear costs.

## Tests (record each red)
1. **Outer, `[realterrainsweep]`**: a third line `REALTERRAIN terraform solved= failed= cost_mean=`: per fixture, the hard-facts placement (Z2b) and then this solve; where both succeed: the terraformed terrain (ApplyJunctionTerraform) passes T3's facts solve with heights (same choices, same positions: SamePlacement) and JunctionRoutesHeightsOk on the render. REQUIRE that for every solved fixture. Red: stub returns no plan (record).
2. **`[topoproj]` "topoproj: a ridge row is cut, not avoided"**: the T on the T3 ridge case: plan non-empty, cost > 0, terraformed terrain passes heights, positions equal the flat solve's.
3. **`[topology]` "topology: the ramp rule spaces up-ramps"**: a synthetic straight edge climbing 3 levels over 20 tiles with k = 1 and tl 4: up-ramps are at least d apart in the solution; with k >= tl they may be adjacent if cheaper.
4. **`[topology]` "topology: flat ground needs no terraform"**: T and LL_RR on flat terrain: empty plan, cost = 0 (+ clear costs, 0 on clear land).

## Changes
1. junction_topology.h: `JunctionTrackPlacement` gains `JunctionTerraform terraform; ///< The height solve's plan (D218), empty on flat ground.` and `Money height_cost = 0; ///< Terraform, foundations and clearing of the chosen levels.`; the facts overload gains `bool terraform = false` after `heights`: when true it forces `heights = false` for X and runs this solve after X (and after the D215 objective if merged).
2. The solve as file-static code in junction_topology.cpp (or a new junction_levels.{h,cpp} if it exceeds ~250 lines: name it so and say so).
3. TESTS.md rows.

## Do not
- No level relations inside JunctionPositions (Z is a separate pass in this step). No edits, no A*. No in-game terraform or build steps. No ramp tokens in the programs yet (Z3: ramps become graph tokens with positions once this measures).

## Stop conditions
- A solved fixture whose terraformed terrain fails heights (report: fixture, tile, edge, shape, the T3 reason) — a finding; terraform conflicts on more than a quarter of fixtures (report: the coupling needs design); anything not covered.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `tools/junction/jt fast build '[topoproj]' 5`; `build-rel/openttd_test` with `"[realterrainsweep]"`, `"[placeoracle]"`, `"[topoprojsweep]"` (to files); `tools/junction/jt gate build-rel` (report the count).
- One commit: `Add: Junction height solve: levels per edge by shortest path, a terraform plan and its cost (D218)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus the three REALTERRAIN lines, the terraform-conflict count, mean and max Release micros of the height solve, and the cost distribution (min/median/max) over solved fixtures. When you have reported, stop.
