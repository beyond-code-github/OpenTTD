# P4b: best-first graph search with the local detour as a graph edit (DESIGN-G1 §13.5, §13.8 items 1-3, 7; D198, D213, D214)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

**Source rule (D213):** a search state is the graph seed plus an edit list; each state is rebuilt from them (routes -> edits applied to the routes -> graph input -> template -> solve). No state is a layout, nothing traces a layout. The solved layout is output; checking it against terrain (heights) is allowed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git checkout -B wt-p4b <HEAD>` (given at launch). Never use git stash.
- Merged: P1-P4a, P3b. The graph pipeline (see the `[topoproj]` tests in src/tests/junction_topology.cpp for the full call chain), SolveJunctionNodes' graph overload, RenderJunctionNodeRoutes, failures with `elements` (tpl.link_tags, tpl.point_tags).
- RECON-detour.md (read it whole): the legacy InsertJunctionDetour (junction_nodes.cpp ~:1998) and SearchJunctionNodes (layout states, detour-count queue, 16 states, Straight/Break -> detour on each side, acceptance by heights and fits).
- Orchestrator rulings on RECON-detour's open questions: (1) an edit is keyed (edge, run, copy) and applies to every route whose piece tags carry that edge and copy; (2) realise edits on the ROUTES (a post-process of the seed's tagged routes), restarting at the template; detour pieces are tagged RUN of the same run; (3) an edit stores the failing piece's index within its run in the seed routes, not a column; (4) a side whose new tiles meet any tile of another route (or a bridge head) is refused for that state; (5) acceptance = the graph solve succeeds and the heights/fits check over the solved routes passes; a failing check drops the state (as today); (6) cost = rail_piece per non-bridge piece + turn_cost per curve over all routes (prices from JunctionUnitPrices/line-router turn_cost as RECON §6 names; if no default instance exists for the test spec, STOP), queue ordered by cost, ties FIFO; (7) at most one edit per (edge, run, copy): a failure on an already-edited run makes no child; (8) max states as given (16 in tests).

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); TESTS.md (`[topoproj]`, `[topoprojsweep]` rows); DESIGN-G1.md §13.8; RECON-detour.md.
- src/junction_nodes.{h,cpp}: InsertJunctionDetour, SearchJunctionNodes, JunctionNodeHeightsOk, the graph solve overload, RenderJunctionNodeRoutes, JunctionNodeFailure.
- src/junction_topology.h: JunctionTrackRoute, JunctionTrackPieceTag; src/tests/junction_nodes.cpp :545-600 (legacy search tests, MakeMapTestTerrain, BlockMapTestTile, SearchFlatT, FlatToWorking); src/tests/junction_topology.cpp the `[topoproj]` and `[topoprojsweep]` tests.

## Tests (tags as named; record each red)
1. **Outer `[topoproj]` "topoproj: LL_RR builds around a house it cannot slide past"**: house = the first tile, in the `[topoprojsweep]` test's deterministic order, whose single-tile solve fails with kind Straight (compute it in the test the same way). SearchJunctionGraph(seed, spec, terrain, offset, 16): REQUIRE a layout; edits non-empty; the house tile (at its offset) holds no track; CheckJunctionRules(layout, spec, topology) clean (INFO each). Red: stub returns no layout.
2. **`[topoproj]` "topoproj: the T graph search builds around a house beside the hub"**: the legacy test's terrain (MakeMapTestTerrain, BlockMapTestTile at hub_x - 1, 0, offset FlatToWorking) with the T graph seed: REQUIRE a layout, states <= 16, checker clean (JunctionTTopology()).
3. **`[topoproj]` "topoproj: the graph search keeps the flat seed on flat terrain"**: T and LL_RR on flat terrain: states == 1, edits empty, tracks equal the graph solve's.
4. **Inner `[topoproj]` "topoproj: a detour edit on routes"**: on LL_RR's seed routes, ApplyJunctionDetours with one edit on the run found in test 1 (its edge, run, copy, piece, side +1 and -1): every changed route stays continuous (each piece's entry meets the previous piece's exit), the edited piece's tile holds no piece of that route, the run gains exactly 4 curves, new pieces are tagged RUN with the run's (edge, run, copy); a side meeting another route's tile returns empty with a reason. Red: stub returns empty.
- All existing tests stay green and unedited (the legacy search is untouched).

## Changes
1. **junction_topology.h/.cpp:**
```cpp
/** The graph pipeline's products for one topology and spec (DESIGN-G1 §13): the seed a projection search starts from. */
struct JunctionGraphSeed {
	const JunctionTopology *topology;
	std::vector<JunctionLanePath> paths;
	JunctionTrackGraph graph;
	JunctionTrackStructure structure; ///< After the bridge cut.
	JunctionTrackPrograms programs;
	JunctionTrackPlacement placement;
	std::vector<JunctionTrackRoute> routes;
};
/** Run the graph pipeline (expand, graph, order, structure, programs, cut, render routes); empty routes and the reason in error on failure. */
JunctionGraphSeed MakeJunctionGraphSeed(const JunctionTopology &topology, const JunctionSpec &spec, std::string *error);

/** A local detour as a graph edit (DESIGN-G1 §13.5): an out-and-back S-bend on one run of one edge, around the piece at index `piece` within the run (in the seed's routes), to side +1 (right of travel) or -1. Position-free: lengths are recomputed when applied. */
struct JunctionDetourEdit {
	uint32_t edge;
	uint16_t run;
	uint8_t copy;
	uint16_t piece;
	int8_t side;
	auto operator<=>(const JunctionDetourEdit &) const = default;
};
/** The routes with every edit applied to every route carrying its (edge, copy) run (D213: realised on routes, not a layout); empty with the reason in why when an edit is refused (not an X run, piece not strictly inside, a shifted run of one piece, no S-bend to that side, a new tile meeting another route or a bridge head). */
std::vector<JunctionTrackRoute> ApplyJunctionDetours(const std::vector<JunctionTrackRoute> &routes, const std::vector<JunctionDetourEdit> &edits, const JunctionSpec &spec, std::string *why);
```
   The detour's shape and lengths (n1, n2, n3: latest out-bend, shortest shifted run covering the piece) as InsertJunctionDetour chooses them: move that choice and its program/raster building into a file-static helper shared by both (legacy behaviour unchanged; its tests stay green), or, if they live in different files, a helper declared in junction_rules_internal.h. If sharing needs a behaviour change in the legacy function, STOP.
2. **junction_nodes.h/.cpp:**
   - JunctionNodeSolve gains `TileIndexDiffC origin{}; ///< Graph overload: the template-frame tile of the layout's (0, 0) (the normalisation shift).` set by the graph overload.
   - `bool JunctionRoutesHeightsOk(const std::vector<JunctionTrackRoute> &routes, const JunctionLayout &layout, TileIndexDiffC origin, const JunctionTerrain &terrain, TileIndexDiffC offset, std::string *why);` = JunctionNodeHeightsOk's per-piece fit and join-height checks with the routes (template frame) as the paths and the layout's tracks (at tile - origin) as the tile bits; plus the legacy search's fits loop over the layout's tiles if it checks more than this (read it; fold it in).
   - The search:
```cpp
/** One projection search (DESIGN-G1 §13.8): best-first by cost over edit lists (D198), each state rebuilt from the seed. */
struct JunctionGraphSearch {
	std::optional<JunctionLayout> layout;
	std::vector<JunctionDetourEdit> edits; ///< Of the accepted state.
	int states = 0;
	std::string log;
};
JunctionGraphSearch SearchJunctionGraph(const JunctionGraphSeed &seed, const JunctionSpec &spec, const JunctionTerrain &terrain, TileIndexDiffC offset, int max_states);
```
   Per state: routes = ApplyJunctionDetours(seed.routes, edits) (refused: skip the state); input = JunctionNodeInputOfGraph(...); tpl; solve (graph overload) on JunctionFacts(terrain) at offset; success and JunctionRoutesHeightsOk(RenderJunctionNodeRoutes(tpl, input, values), layout, origin, ...) -> accept. Failure of kind Straight or Break with an element (edge, run, copy) and no edit on that run yet: map the failure's working column to the piece index within that run in the SEED routes (column - offset.x against the run's flat tiles; if it falls outside the run, no child) and push one child per side. Visited key = sorted edits. Log one line per state (edits, cost, outcome).

## Do not
- Do not change the legacy search, InsertJunctionDetour's behaviour, the template rules, the snap, the leans, GenerateJunctionNear, the GUI.
- No other repair kinds (ramp P4c; terraform, tunnels P6).

## Stop conditions
- Test 1's house gives no Straight failure in the sweep order (report the kinds seen); the search finds no layout (report the log's last 30 lines); checker violations (each); any recipe item needing a layout read; an existing test fails; anything not covered.

## Gates
- `tools/junction/jt fast build '[topoproj]' 5`; `tools/junction/jt fast build '[topology]' 5`; `tools/junction/jt fast build '[nodes]' 5`; `tools/junction/jt gate build-rel` (report the count). If `[topoproj]` goes over its budget, move test 1 to `[topoprojsweep]` and say so.
- One commit: `Add: Junction projection search over graph edits with the local detour`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. TESTS.md rows updated.

## Report
The standard report, plus: test 1's house tile, the accepted edits, states and Release time; test 2's states and time. When you have reported, stop.

## Amendment 1 (orchestrator, after the stop: house column outside the seed run; detour curve gap)
1. **The house column:** the edit stores the failure's working column (a world tile column: fixed for a given offset), not a piece index: `JunctionDetourEdit::piece` becomes `int32_t column; ///< The failing tile's working x (the house is fixed in the world).` ApplyJunctionDetours takes the offset and maps column - offset.x into the seed run, CLAMPED strictly inside it (the seed run is only the reference shape; the solve moves the detour, and the blocked tile's facts keep the original row off it). Test 1 keeps the sweep's first Straight house.
2. **Curve gap by construction:** ChooseJunctionDetour's candidates (n1, n2, n3) are filtered by the curve rule: for a candidate, take the edited route's full curve-hand sequence (as the template's rule 2 reads curves: 45-degree direction changes) and keep only candidates for which every JunctionCurveGaps pair holds on the units of the edited route; the first surviving candidate in the existing order wins; none: refuse with a reason. No template build inside the edit. If the legacy InsertJunctionDetour's results would change by sharing this filter, keep the filter in the graph path only (a parameter to the shared helper) and say so.
3. **Reds:** record them properly: run each new test once against stubs (ApplyJunctionDetours returning empty, SearchJunctionGraph returning no layout) before restoring the code.
