# HF1b: the node solve meets the height joins (D194, DESIGN-N3 §10)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (persistent; Debug `build`, Release `build-rel`; incremental builds). First: `git checkout -B wt-hf1b <HEAD>` (given at launch). You are the only writer there.
- HF1a (merged) made the tile facts ask the game: `JunctionFacts::Fits(x, y, bits)`, `EdgeZ(x, y, bits, edge)` (pixel z at an edge midpoint with the tile's whole track set built), Straight's entry/exit in pixel z, and the oracle JunctionNodeHeightsOk checks the join rule (consecutive pieces meet at equal EdgeZ). This step puts the same join rule into the solve so the oracle never rejects (D193/D194: correct by construction, no trial-and-error repairs).
- Design: docs/junction-tool/DESIGN-N3.md §10 (bullets "Joins", "Y snap with facts"; the "Height of a tile" bullet is superseded by the Revision bullet: heights are EdgeZ). Measured problem: docs/junction-tool/DIAG-node-shortfalls.md Q1 (ridge at offset (0,0): a stair half and a merge half on the ramp row, rejected only by the oracle).

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); docs/junction-tool/TESTS.md (jt: tags bracketed).
- DESIGN-N3.md §1, §5, §6, §10.
- src/junction_facts.h (whole).
- src/junction_nodes.h :20-195; src/junction_nodes.cpp: BuildJunctionNodeTemplate and the path tracing it uses, NodeDiagonalSteps/NodeDiagonalRaster, AddJunctionNodeLateConstraints (the diagonal equality k), SolveJunctionNodes whole (2b Y snap, 2c, 2d, step 3 per-class filter incl. the Y-free column check and the diagonal interior block, step 4 X-free links with `d` per link), JunctionNodeHeightsOk (the join rule you must agree with).
- src/tests/junction_game.cpp: SetNearRampRow, the ridge test [tmapnear], the [nodeoracle] test.

## Design (settled; implement exactly)
1. **Join height**: a static helper `static bool NodeJoinMeets(const JunctionFacts &facts, int ax, int ay, TrackBits a_bits, Trackdir a_td, int bx, int by, TrackBits b_bits)`: `e = TrackdirToExitdir(a_td)`; true iff both EdgeZ are not INT_MIN and `facts.EdgeZ(ax, ay, a_bits, e) == facts.EdgeZ(bx, by, b_bits, ReverseDiagDir(e))`. Bits are the tile's whole track set: for template pieces from `flat_bits` (the flat render's bits per tile, as step 3 uses), for a free X or Y run's interior tile the lone axis track, for a diagonal raster tile its lone track.
2. **Joins in the template**: `struct JunctionNodeJoin { TileIndexDiffC from_tile; Trackdir from_td; TileIndexDiffC to_tile; uint16_t cls; uint16_t ycls; };` (doc: "Two consecutive rigid pieces of one group (same X and Y class) that must meet at one height (DESIGN-N3 §10, kind a); tiles in the flat layout.") and `JunctionNodeTemplate::joins` (deduplicated by from_tile, from_td, to_tile) and `JunctionNodeTemplate::open_joins` (`uint32_t`, "Consecutive pieces of different classes that are not the two ends of one free link: left to the oracle"). Filled in BuildJunctionNodeTemplate from the traced paths: consecutive pieces, neither on_bridge, neither strictly inside a free link: same cls and ycls -> a join; else, unless they are the ends of one free link, `open_joins++`.
3. **Failure kind**: `JunctionNodeFailure::Kind::Height` appended, doc "No row for a Y class meets the facts and heights of its pieces (DESIGN-N3 §10); cls is the Y class." Update the `cls` member doc.
4. **Y snap with facts** (step 2b): move `flat_bits` construction above 2b. For Y classes, a candidate t (nearest-flat order, as now) is accepted only if `RestrictRange(c, t, t)` succeeds and `NodeYFits(c)` holds, else Undo and try the next; no value -> `Kind::Height` with cls = c. `NodeYFits(c)`: (i) for every rigid group with ycls == c (pieces grouped by cls, on_bridge excluded): some x of the group's X class in [Lo, Up] clamped to the snapshot (working x from facts.MinX() to facts.MaxX(), converted as step 3 converts) at which every piece passes JunctionNodeTrackFits with its flat_bits and every join of the group meets; (ii) every free diagonal whose two end Y classes are now both fixed and one of them is c: some x of p.cls at which its interior raster tiles Fit (lone track) and the chain p -> raster[1] -> ... -> raster[n-2] -> q meets (q's x = x_p + k, k as AddJunctionNodeLateConstraints computes it); (iii) every Y-free link whose two end Y classes are both fixed now and one of them is c: some x of its X class at which the column check of step 3 holds and p -> first interior, last interior -> q meet (p -> q directly when there is no interior). No backtracking.
5. **X filter** (step 3, class c, value v), in addition to what it checks now: every join of class c meets; each free X run (non-diagonal X-free link) with p.cls == c: p -> the lone X straight at x_p + d meets, and with q.cls == c: the lone X straight at x_q - d -> q meets (d per link as step 4 computes it; conservative when the run ends up with no interior: say so in a comment); each Y-free link of class c: the column check also requires its end joins (as 4 iii); each free diagonal with p.cls == c: the chain of 4 ii meets.
6. Nothing else. The search's post-solve heights and fits checks stay (oracle).

## Tests (outermost first; record each red)
1. Outer: the ridge test [tmapnear]: replace `WARN("ridge: offset " ...)` by `CHECK(near.offset.x == 0); CHECK(near.offset.y == 0);`. Red now: (-2,-2).
2. Outer: [nodeoracle]: count search log lines containing "heights:" and "fits:" over all terrains, print them on the ORACLE line (`heights=`, `fits=`), `CHECK(heights == 0)`. Record the red count.
3. Inner `[nodes]`: `"the Y snap moves a stair off a ramp row"`: the test-spec T template; facts over a terrain with the ridge test's four rows (build_on_slopes true, slopes via InclinedSlope as SetNearRampRow now sets them) at the working rows the ridge test has at its (0,0) start offset; SolveJunctionNodes; REQUIRE a layout; CHECK JunctionNodeHeightsOk (layout offset as SearchJunctionNodes computes it). Red now: heights false.
4. Inner `[nodes]`: `"a split whose foundation does not meet its run moves"`: the test-spec T on flat terrain, build_on_slopes true; take the first free X link whose p tile has two or more tracks; give p's working tile a slope for which `Fits(p's bits)` is true and `EdgeZ` at the edge toward the run differs from the flat run's z (find it in the test by looping over the 15 non-steep slopes with the facts; REQUIRE one exists); solve; if a layout comes back, CHECK JunctionNodeHeightsOk. Red now: false.
5. `[nodes]`, `[nodespeed]`, `[tmapnear]`, `[facts]`, `[positions]` stay green; no existing expectation edited (if one flips, STOP and report).

## Do not
- No search children, repairs, backtracking, terraforming, bridge or tunnel heights. Do not change the X snap, occupancy, late constraints, the oracle, or the facts.
- Do not apply docs/junction-tool/hf1-wip.patch or yr1-wip.patch.

## Stop conditions
- Test 1 or 2 still red after the change: report the ridge search log at (0,0) (scratch print, removed after), the first 10 remaining "heights:" lines with seeds, and `open_joins` for the test-spec T; stop without committing.
- `[nodes]` Release over 1 s, `[nodeoracle]` Release over 5 s, the ridge BENCH line over 50 ms.

## Gates
`tools/junction/jt fast build "[nodes]" 5`, `tools/junction/jt fast build-rel "[nodes]" 1`, `tools/junction/jt fast build-rel "[nodespeed]" 5`, `tools/junction/jt fast build-rel "[tmapnear]" 5`, `tools/junction/jt fast build-rel "[facts]" 1`, `tools/junction/jt gate build-rel` (budget 120 s). Update TESTS.md times for tags you ran. Commit: `Feature: Junction node solve meets the height joins (D194)`, body with the ridge result (offset, BENCH micros), the ORACLE line before and after, open_joins, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
References read; commits; red evidence per test; types and members added/removed; suite results with times; ORACLE lines; open_joins; stops; `git status --porcelain` and `git log -1`. When you have reported, stop.
