# HF1: height facts in the node solve (D194, DESIGN-N3 §10)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (persistent; Debug `build`, Release `build-rel`; incremental builds). First: `git checkout -B wt-hf1 <HEAD>` (given at launch). You are the only writer there.
- Decisions: D194 (height facts go into the solve as designed; no trial-and-error repairs), D193 (correct by construction). Design: docs/junction-tool/DESIGN-N3.md §10 (read it twice; it is the spec).
- Measured problem (docs/junction-tool/DIAG-node-shortfalls.md Q1): over the near-test ridge at offset (0,0) the solve places a stair half and a merge's half on the ramp row; only the post-solve JunctionNodeHeightsOk rejects it. YR1 (row moves by retry) was NOT ADOPTED; do not revive it.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); docs/junction-tool/TESTS.md (jt usage: tags in brackets, e.g. `tools/junction/jt fast build-rel "[nodes]" 1`).
- docs/junction-tool/DESIGN-N3.md §1, §5, §6, §10.
- src/junction_facts.h (whole) and src/junction_facts.cpp constructor (~:30-60: entry/exit, UsableAt at z_max).
- src/junction_nodes.h :20-130 (template structs) and :130-170 (failure, solve, heights).
- src/junction_nodes.cpp: BuildJunctionNodeTemplate (grep it; how paths are traced into points and pieces), NodeDiagonalSteps/NodeDiagonalRaster, SolveJunctionNodes whole (step 2b Y snap `snap` lambda, 2c late, 2d occupancy, step 3 per-class filter incl. the Y-free column check and DF1's diagonal block), JunctionNodeHeightsOk and NodeHeightStep (the oracle's rules: these define what "meets at one height" means; your edge heights must agree with them).
- src/tests/junction_game.cpp :500-545 (SetNearRampRow, the ridge test) and the [nodeoracle] test.

## Design (settled; implement exactly)
1. **JunctionFacts::Plane**: `int8_t Plane(int x, int y) const; ///< The tile's z_max: the height of a switch or half piece on it (flat or on a foundation). INT8_MIN off the snapshot.` Backed by a new `std::vector<int8_t> plane;` filled in the constructor.
2. **Joins in the template**: 
   ```cpp
   /** Two consecutive tiles of a template path that must meet at one height (DESIGN-N3 §10). */
   struct JunctionNodeJoin {
   	TileIndexDiffC from_tile; ///< In the flat layout.
   	Trackdir from_td;
   	TileIndexDiffC to_tile;
   	Trackdir to_td;
   	uint16_t cls; ///< The X class of both tiles (kind a).
   	uint16_t ycls; ///< The Y class of both tiles (kind a).
   };
   ```
   `JunctionNodeTemplate::joins` (`std::vector<JunctionNodeJoin>`, doc "Kind (a) joins: consecutive rigid pieces of one group, deduplicated by (from_tile, from_td, to_tile, to_td)") and `JunctionNodeTemplate::open_joins` (`uint32_t`, doc "Joins between two different classes that no rule of DESIGN-N3 §10 covers (left to the oracle)"). Filled in BuildJunctionNodeTemplate from the traced paths: for each path and each consecutive pair of its pieces where neither is on_bridge and neither is strictly inside a free link: same cls and same ycls -> a join; different classes and not the two ends of one free link (kinds b, c, d handle those) -> `open_joins++`.
3. **Edge heights** (static helper in junction_nodes.cpp): `static bool NodeTileHeights(const JunctionFacts &facts, int x, int y, Trackdir td, TrackBits bits, int8_t &entry, int8_t &exit)`: bits.Count() >= 2 or td not along an axis -> entry = exit = facts.Plane(x, y) (return false when Plane is INT8_MIN); otherwise `facts.Straight(x, y, axis, e, x2)` and for travel along -axis swap them; return what Straight returns. Check it against NodeHeightStep on the oracle's rules; if they disagree on any case, STOP and report the case.
4. **Y snap with facts** (step 2b): replace the plain `snap(c)` for Y classes by a snap whose candidate value t (nearest-flat order, as now) is accepted only if, after `RestrictRange(c, t, t)` succeeds, the predicate `NodeYFits` holds; otherwise Undo and try the next. `NodeYFits` (a lambda or static) for class c at t: (i) for every rigid group (pieces with ycls == c, grouped by cls, on_bridge excluded): some x value in [Lo(cls), Up(cls)] ∩ snapshot (facts.MinX()-offset.x .. facts.MaxX()-offset.x in layout terms; compute it in working x as step 3 does) at which every piece of the group passes JunctionNodeTrackFits (with flat_bits, exactly as step 3) and every join of kind (a) of that group meets (NodeTileHeights exit of from == entry of to); (ii) for every free diagonal whose p.ycls and q.ycls are both fixed now (Lo == Up) and one of them is c: some x of p.cls in its range at which the (d) chain meets (raster at the length from the now-known Y values; tiles Half-fit as DF1; heights p -> raster[1] -> ... -> q, with q's x = x_p + k, k as in AddJunctionNodeLateConstraints); (iii) for every free Y run whose two ends' ycls are both fixed now and one is c: some x of its X class at which the column check (as step 3) and the (c) end joins meet. Move flat_bits' construction above step 2b so both steps use it. When no value is accepted: `result.failure = JunctionNodeFailure{Kind::Height, UINT32_MAX, 0, static_cast<uint16_t>(c)}`.
5. **JunctionNodeFailure::Kind::Height** (append after Occupancy): doc "No row for a Y class meets the facts and heights of its pieces (DESIGN-N3 §10); cls is the Y class." Update the `cls` member's doc to mention Height.
6. **X filter** (step 3, per class c and value v): in addition to what it checks now: every kind (a) join of class c meets; for each free X run (non-diagonal X-free link) with p.cls == c: p's tile -> X straight at x_p + d meets (d = the path's dir for that link, as step 4 computes it); with q.cls == c: X straight at x_q - d -> q meets; each Y-free link of class c: the column check also requires p -> first interior and last interior -> q to meet (p -> q when there is no interior); each free diagonal with p.cls == c: the (d) chain meets (DF1's block already has the raster tiles; add the heights).
7. Nothing else. The search's post-solve heights and fits checks stay (oracle).

## Tests (outermost first)
1. Outer, red: the ridge test [tmapnear] in src/tests/junction_game.cpp: replace `WARN("ridge: offset " ...)` by `CHECK(near.offset.x == 0); CHECK(near.offset.y == 0);`. Red now: offset (-2,-2).
2. Outer, red: the [nodeoracle] test: also count, over all terrains, search log lines containing "heights:" and "fits:" (the post-solve rejections), print them on the ORACLE line (`heights=`, `fits=`), and `CHECK(heights_rejections == 0)`. Record the red count. (`fits=` is printed, not checked.)
3. Inner, `[facts]` (src/tests/junction_facts.cpp or wherever the [facts] tests live; grep): `"Plane is a tile's z_max"`: a 3x1 terrain with tiles flat z0, inclined z0..1, flat z2: Plane gives 0, 1, 2. Red: does not compile, then fails with the stub returning 0 if you add a stub first (add the member returning 0 first to get the assertion failure).
4. Inner, `[nodes]` in src/tests/junction_nodes.cpp: `"the Y snap moves a stair off a ramp row"`: the test-spec T template; flat facts over a terrain with the four ridge rows of the ridge test (copy the four rows: z ranges and inclines as SetNearRampRow sets them; rows at the same working rows as the ridge test at its (0,0) start offset); `SolveJunctionNodes`; `REQUIRE(solve.layout)`; `CHECK(JunctionNodeHeightsOk(*solve.layout, spec, terrain, layout_offset, &why))` with layout_offset computed as SearchJunctionNodes computes it. Red now: heights false (path 4 piece at 44,9).
5. Inner, `[nodes]`: `"a switch on a foundation beside a flat run moves"`: the test-spec T template on flat terrain (build_on_slopes true); pick the first free X link whose p tile holds two or more tracks (a switch); set p's working tile to z_min 0, z_max 1, incline along Y (DiagDirection::SE) so the switch would sit on a foundation at 1 beside z0 tiles; solve; if a layout comes back, CHECK JunctionNodeHeightsOk is true. Red now: false.
6. `[nodes]`, `[nodespeed]`, `[tmapnear]`, `[facts]`, `[positions]` stay green.

## Do not
- Do not add search children or repairs of any kind. Do not change the X snap, the occupancy chain, the late constraints, the search, or JunctionNodeHeightsOk's rules.
- Do not add backtracking to the Y snap.
- Do not model bridges' or tunnels' heights.

## Stop conditions
- NodeTileHeights disagrees with the oracle's rules (item 3).
- Test 1 or 2 still red after the change: report the ridge search log at (0,0) and the oracle's remaining "heights:" lines (first 10, with seed), plus `open_joins` for the test-spec T, and stop without committing.
- `[nodes]` Release over 1 s, `[nodeoracle]` Release over 5 s, ridge GenerateJunctionNear over 50 ms (BENCH line).
- `open_joins` > 0 for the test-spec T: record the count and the joins (tiles) in the report; this is not a stop unless test 1 or 2 fails because of one.

## Gates
`tools/junction/jt fast build "[nodes]" 5`, `tools/junction/jt fast build-rel "[nodes]" 1`, `tools/junction/jt fast build-rel "[nodespeed]" 5`, `tools/junction/jt fast build-rel "[tmapnear]" 5`, `tools/junction/jt fast build-rel "[facts]" 1`, `tools/junction/jt gate build-rel` (budget 120 s). Update docs/junction-tool/TESTS.md times for the tags you ran. Commit: `Feature: Junction node solve meets the height facts (D194)`, body with the ridge result (offset, BENCH micros), the ORACLE line before and after, open_joins, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
References read; commits; red evidence per test; types and members added/removed; suite results with times; ORACLE lines; open_joins; stops; `git status --porcelain` and `git log -1`. When you have reported, stop.
