# YR1: a heights failure moves the failing piece's node row (Y pin repair)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (persistent; Debug `build`, Release `build-rel`, incremental builds). First: `git checkout -B wt-yr1 <HEAD>` (HEAD given at launch). You are the only writer there.
- Why (docs/junction-tool/DIAG-node-shortfalls.md Q1): over the near-test ridge at offset (0,0) the search solves one state, the layout is rule-clean, and JunctionNodeHeightsOk rejects it: path 4's half piece and path 6's S-bend stair sit on the ridge's ramp row, where a path can only climb or drop on a straight axis piece. The Y snap puts every Y class at its flat row with no facts (DESIGN-N3 §5), and the search repairs only Straight/Break failures (detours), so nothing moves the stair off the ramp row. The Y classes already have freedom (Y-free links, depth is a variable; DESIGN-N3 §5): this step lets the search use it. Scope: heights failures only (the measured case). Piece failures get no Y children in this step.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); docs/junction-tool/DESIGN-N3.md §4, §5, §6; docs/junction-tool/TESTS.md.
- src/junction_nodes.h :100-195 (template, JunctionNodeFailure, JunctionNodeSolve, SolveJunctionNodes, JunctionNodeHeightsOk, JunctionNodeSearch, SearchJunctionNodes).
- src/junction_nodes.cpp: SolveJunctionNodes up to step 2b "The Y variables: snapped to the flat rows" (~:956-1000); RenderJunctionNodeTracks (how a piece, a diagonal's interior raster and a free link's interior are placed: `tile + values - flat` per class); JunctionNodeHeightsOk (~:1189-1232; the `why` formats at ~:1201 and ~:1229); NodeState, NodeStateKey and SearchJunctionNodes (~:1323-1460).
- src/tests/junction_game.cpp: the ridge test "GenerateJunctionNear builds the projected T map over a ridge" [tmapnear] and its helpers (MakeNearTestTerrain, SetNearRampRow).
- The existing tests of JunctionNodeHeightsOk and SearchJunctionNodes in src/tests/junction_nodes.cpp (grep both names).

## Design (settled; implement exactly)
- A **Y pin** restricts one Y class of a state's template to a range: `struct JunctionNodeYPin { uint16_t ycls; int lo; int hi; };` (in junction_nodes.h beside JunctionNodeSolve, doc comment: "A Y class of the state's template held in [lo, hi] rows (layout frame of the template); the search's row-move repair (YR1)."). Pins are valid only with the template they were made for; a child state with pins has the SAME layout as its parent, so the template rebuilt from it has the same class indices.
- **SolveJunctionNodes** gains a last parameter `std::span<const JunctionNodeYPin> pins = {}`. Right before step 2b (the Y snap), for each pin: `positions.RestrictRange(pin.ycls, pin.lo, pin.hi)`; if false: `result.failure = JunctionNodeFailure{Kind::Template}` and return (as the other template failures do). The snap then picks the nearest allowed value as it already does.
- **JunctionNodeHeightsOk** gains a last parameter `TileIndexDiffC *bad_tile = nullptr`: set to the working tile (the same x, y printed in `why`) where the per-piece failure is formatted (~:1201). The other failure (~:1229) does not set it.
- **NodeYClassAt** (static in junction_nodes.cpp, above SearchJunctionNodes): `static std::optional<uint16_t> NodeYClassAt(const JunctionNodeTemplate &tpl, const std::vector<int> &values, int x, int y)` with x, y in the template's layout frame (working minus the state offset). Returns, checking in this order: the `ycls` of a non-on_bridge piece rendered at (x, y) (`piece.tile + (values[cls] - flat[cls], values[ycls] - flat[ycls])`); the first point's `ycls` of a free diagonal one of whose raster interior pieces is rendered at (x, y) (raster at `diag.halves + 2 * NodeDiagonalSteps(tpl, diag, values)`, placed as RenderJunctionNodeTracks places it); the from point's `ycls` of an X-free (non-diagonal) link whose interior covers (x, y); otherwise nullopt (a Y-free link's interior is a straight Y piece: no row to move).
- **Search**:
  - NodeState gains `std::vector<JunctionNodeYPin> pins;` and `int moves = 0;` (doc: "Row moves made by the YR1 repair."). The queue key becomes `detours + moves`. Detour children copy neither pins nor moves (a detour changes the layout and so the template: pins of the old template are dropped; moves carry over as a cost: child moves = parent moves).
  - The visited set becomes `std::set<std::pair<std::vector<std::tuple<int, int, uint>>, std::vector<std::tuple<int, int, int>>>>`: the layout key and the sorted pins as (ycls, lo, hi).
  - SolveJunctionNodes is called with `state.pins`.
  - The state log head becomes `"state {} detours {} moves {}: "`.
  - On a heights failure: call JunctionNodeHeightsOk with `&bad` (a TileIndexDiffC). If bad was set: `t = bad - state.offset` (template layout frame; check this against the solve: the render before normalisation is in the template frame and working = template frame + state.offset; if the anchor-shift arithmetic says otherwise, STOP and report). `c = NodeYClassAt(tpl, solve.values, t.x, t.y)`. If c has a value and `*c != tpl.anchor_y`: for side in {-1, +1}: the child's pins are the parent's pins with the pin for `*c` replaced (or added) by `[v + 1, 1024]` for side +1 or `[-1024, v - 1]` for side -1, where `v = solve.values[*c]`; child = same layout, same offset, same detours, `moves + 1`; skip if visited; push. Log one line per child: `"  move ycls {} side {}\n"`.
  - Nothing else in the search changes. Max states stays the caller's.

## Tests (outermost first)
1. Outer, red: in src/tests/junction_game.cpp, the ridge test [tmapnear]: replace `WARN("ridge: offset " ...)` by `CHECK(near.offset.x == 0); CHECK(near.offset.y == 0);` (keep the rest). Red before the change: offset is (-2,-2). Record the failure message.
2. Inner, `[nodes]` in src/tests/junction_nodes.cpp: `"a Y pin moves its class a row"`: the test-spec T template (as the existing flat solve test builds it), flat facts; pick the first Y class `c` (index >= tpl.x_classes) that is not `tpl.anchor_y` and is the ycls of some point that has a Y-free link (grep the template for `link.axis == Axis::Y && link.free`); solve with the pin `{c, flat[c] + 1, flat[c] + 1}`; `REQUIRE(solve.layout)` and `CHECK(solve.values[c] == tpl.flat[c] + 1)`. Red before the change: does not compile; after adding the parameter but before applying the pins, the CHECK fails with values[c] == flat[c]. Record the latter.
3. Inner, `[nodes]`: `"a heights failure names its tile"`: extend or copy the nearest existing JunctionNodeHeightsOk failure test: pass `&bad` and `CHECK` it equals the x, y printed in `why`. Red: bad stays at its initial value {-1, -1} before the change (after the parameter exists).
4. `[nodes]`, `[nodespeed]`, `[tmapnear]`, `[nodeoracle]` stay green.

## Do not
- Do not add Y children for Piece, Straight, Break, Occupancy, fits or any failure other than heights. Do not change the Y snap's choice rule, the detour code, GenerateJunctionNear's offsets, or the max-states cap.
- Do not model ramps or heights in the solve.

## Stop conditions
- The outer test is still red after the change: report the ridge search's full log at offset (0,0) (call SearchJunctionNodes the way GenerateJunctionNear does, in a scratch test you delete afterwards) and stop without committing. Do not raise the state cap or add children of other kinds.
- `[tmapnear]` Release over 5 s, `[nodes]` Release over 1 s.
- The frame arithmetic for `t` does not hold (see Design).

## Gates
- `tools/junction/jt fast build nodes 5`, `tools/junction/jt fast build-rel nodes 1`, `tools/junction/jt fast build-rel nodespeed 5`, `tools/junction/jt fast build-rel tmapnear 5`, `tools/junction/jt gate build-rel` (budget 120 s; note the ORACLE line).
- Commit: `Feature: Junction node search moves a node row on a heights failure`, body with the ridge result (offset, states, Release time from the BENCH line) and the ORACLE line, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
References read; commits; red evidence per test; types and members added/removed; suite results with times; ridge: states and micros; ORACLE line; stops; `git status --porcelain` and `git log -1`. When you have reported, stop.
