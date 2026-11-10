# M1: the template junction as a map, projected (DESIGN-O4 revision 5, D170, D170a)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/m1`, branch `wt-m1`, made from `feature/junction-tool` at the head given at launch. Build directory: create `build` in the worktree, configure like the main checkout's (`/home/pete/OpenTTD/build`, check its CMakeCache for the generator and build type), build target `openttd_test` only.
- Plan: main checkout `docs/junction-tool/DESIGN-O4.md` "Revision 5: the theory" (lines 1-35).
- Why: the old generator (`BuildJunctionCandidateWithGap` in `src/junction_generator.cpp`) is rule-clean today, and it is already a map projected by routing, written procedurally: fixed geometry (straight E line, stubs, doubled W lanes from the skeleton solver or the lane search), then four movements routed between start and goal windows derived from the rules, with fences. This step moves that procedure into data, a `JunctionMap`, plus one generic projector, and proves the projected layout is identical to the old one. Later steps open the map's choices (crossings, orders, positions) one at a time.

## Test speed (engineer: "we must have a test suite that runs in seconds not minutes")
- All new tests carry the tag `[junctionmap]` and the whole tag runs in under 5 s on this machine (measure with the test binary's `--durations yes`; report the total).
- While developing, run only `[junctionmap]`. Run the wider gate once, at the end.

## Read first (once each)
- `CODINGSTYLE.md`.
- `src/junction_generator.cpp` lines 120-330 (helpers, `MakeJunctionFrame`, `BuildParallelogramLanes`) and 324-760 (`BuildJunctionCandidateWithGap`, `BuildJunctionCandidate`), whole.
- `src/junction_generator.h`, `src/junction_router.h` (`JunctionRouteRequest`, `JunctionRouteEnd`), `src/junction_skeleton.h`.
- `src/tests/junction_generator.cpp` lines 1-66 (specs) and 127-180 (how candidates are built in tests).

## The map (`src/junction_map.h`, `src/junction_map.cpp`, new; register both in `src/CMakeLists.txt` next to the other junction files)
```cpp
/** What a map node is (DESIGN-O4 revision 5). */
enum class JunctionMapNodeKind : uint8_t { Port, Split, Merge };

/** A place on the map: the pieces it may be projected to, in tile units. */
struct JunctionMapNode {
	JunctionMapNodeKind kind;
	std::vector<JunctionRouteEnd> window; ///< The pieces this node may take; one piece when fixed.
	bool plain_only = false; ///< Only window pieces whose tile holds exactly the piece's axis track in the layout so far count (the old PlainRowPieces filter).
};

/** pos(a).x - pos(b).x >= k, between two nodes (difference constraint along x, as the skeleton solver's). */
struct JunctionMapConstraint {
	uint8_t a;
	uint8_t b;
	int k;
};

/** A stretch of track between two nodes. */
struct JunctionMapEdge {
	std::optional<JunctionRoute> fixed; ///< Exact geometry (main line, stubs, doubled lanes): laid as given, nodes ignored.
	uint8_t from = 0; ///< Start node (its window gives the route's starts).
	uint8_t to = 0; ///< End node (its window gives the route's goals).
	std::vector<TileIndexDiffC> fence; ///< Tiles this edge may not use (RouteFencedAndAdd's fence).
	std::vector<TileIndexDiffC> crossable; ///< As JunctionRouteRequest::crossable.
	Money crossable_cost = 0;
};

/** A junction's structure in tile units: nodes, constraints between them, edges in projection order. */
struct JunctionMap {
	std::vector<JunctionMapNode> nodes;
	std::vector<JunctionMapConstraint> constraints;
	std::vector<JunctionMapEdge> edges;
	JunctionRouteRequest base; ///< Router settings shared by every edge (box, curve spacing, bridges, prices, terrain, plane, cache).
};

/** The result of projecting a map. */
struct JunctionMapProjection {
	std::optional<JunctionLayout> layout; ///< Unfinished (not cropped, no signals).
	std::vector<std::optional<JunctionRouteEnd>> placed; ///< Per node, the piece it was projected to.
	std::vector<JunctionRaster> rasters; ///< Per edge, what was laid.
	std::string error; ///< Which edge failed, when it did.
};

/** Lay a map's edges in order: fixed edges as given; routed edges from their start node's window to their end node's window, both filtered by plain_only and by every constraint whose other node is already placed. */
JunctionMapProjection ProjectJunctionMap(const JunctionMap &map, JunctionLayout layout = {});
```
Projector rules:
- A routed edge's starts are its `from` node's filtered window, its goals its `to` node's filtered window (the old code's `starts`/`goals`). After the route, `placed[from]` = the route's first piece and `placed[to]` = its last piece, unless already placed.
- Constraint filtering: for a window piece w of node n, keep it only if every constraint (a, b, k) with a == n and b placed holds `w.tile.x - placed[b].tile.x >= k`, and with b == n and a placed holds `placed[a].tile.x - w.tile.x >= k`.
- Route with the same calls the old code uses (`RouteAndAdd` / `RouteFencedAndAdd`: fenced when `fence` is not empty), so behaviour is identical. Fixed edges are rasterised and added as the old code does.
- An edge with no route: `error` = "edge <i>: no route", layout unset. (No negotiation rounds in this step: the template's fences keep edges apart, so the first round has no conflicts. Negotiation arrives with the step that opens crossing choices.)

## The template as a map (`src/junction_generator.cpp`, declared in `junction_generator.h`)
```cpp
/** The old template's structure as maps, one per variant the old code builds (style 1: per stagger; style 0: one), with pass 1's crossing zone measured by a trial projection. */
std::vector<JunctionMap> BuildJunctionTemplateMaps(const JunctionSpec &spec, const JunctionCandidateParams &params, const JunctionTerrain *terrain, int gap);
```
- Translate `BuildJunctionCandidateWithGap` decision by decision. `base` = the old `base` request. Fixed edges: the E line, the two stubs, the W row (pass 1) or the doubled lanes (pass 2). Routed edges in the old order E->S, W->S, N->E, N->W, each with its old fences, crossable tiles and costs, and nodes whose windows are the old starts/goals:
  - E->S split: row 0, x 2..xI-2, `plain_only`. Goal: `goal_s`.
  - W->S split: the old W-row and b-row plain pieces (or the single `sl_x` piece), `plain_only`. Goal: the S stub pieces from `ys`.
  - N->E: start `start_n`; merge window row 0, x from xO to width-3, `plain_only`, plus the constraint merge_NE - split_ES >= cover + e_after (the old `std::max(xO, x_s + cover + e_after)`; keep the max by the window's own lower end).
  - N->W: starts the N stub pieces from ys+1; goals row nw_row, x nw_min_x..nw_max_x, `plain_only`.
- Pass 1 (the crossing zone) is a trial projection of the pass-1 map inside `BuildJunctionTemplateMaps`, collecting the zone exactly as `note_crossings` does. Name it in a comment as the one remaining placement-by-trial, to be replaced by crossing positions on the map in a later step.
- Then rewrite `BuildJunctionCandidateWithGap` as: maps = `BuildJunctionTemplateMaps(...)`; for each map, `ProjectJunctionMap`, then the old `finish` (moved to a static function `FinishJunctionCandidate` with the same parameters it captures, behaviour unchanged), then the old best-of selection. Keep the old procedural body in a static function `BuildJunctionCandidateWithGapProcedural` until the equality test passes, then delete it in the same commit (the test keeps a frozen copy of nothing; see test 1).

## Tests (`src/tests/junction_map.cpp`, new, register in `src/tests/CMakeLists.txt`; tag `[junctionmap]`)
Outermost first, red first (red = assertion failure, not a compile error: add the declarations with stub bodies returning empty/nullopt first).
1. "the template map projects to the old layout": for `MakeTestJunctionSpec()` and the pinned spec (copy `PinnedTestSpec` from `tests/junction_generator.cpp` into this file as a static function), at the height the generator picks for each spec (read it from `GenerateJunction(spec)`'s result, or the first height the tests at 127-180 use; say which in your report), for params: crossing_cost 0, e_slack 0 and 1, w_slack 0, bridge_margin 3, lane_style 0 and 1 (8 builds): the layout built through the map equals the one built by `BuildJunctionCandidateWithGapProcedural` (tracks, bridges and signals equal after sorting). While both exist, the test calls the procedural one through a test seam: declare `std::optional<JunctionLayout> BuildJunctionCandidateProceduralForTest(...)` in `junction_generator.h` with the same parameters as `BuildJunctionCandidate`, delete both the seam and the procedural body once the test is green, and then change test 1 to assert instead that each of the 8 layouts is identical to a layout captured before deletion: write the 8 captured layouts' `JunctionLayoutToJson` (or the existing layout serialiser; find it with grep) to `src/tests/junction_map_golden.inc` as string literals and compare against them.
2. "a projected template is rule-clean": for both specs, `GenerateJunction(spec)` is rule-clean and its layout equals the golden one for its params (or, if GenerateJunction picks params outside the 8, just rule-clean).
3. "ProjectJunctionMap honours constraints": a hand-made map on a 20x6 box: a fixed straight edge on row 0, a routed edge from node A (row 0, x 2..5, plain_only) to node B (row 3, x 0..19) with the constraint B - A >= 8: the placed B's x - placed A's x >= 8; with the constraint removed the router's cheaper placement violates it (pick A and B windows so that it does, and assert that it does).
4. "ProjectJunctionMap reports the failing edge": a map whose only goal is blocked: error names edge 1, no layout.
5. Print `BENCH map spec=<name> style=<s> e_slack=<e> micros=<us>` per build of test 1 (to the test output file).

## Do not
- Do not change routing, fences, windows, the skeleton solver, the signal pass, the checker, or `GenerateJunction`'s sweep. The output must be identical.
- Do not add negotiation, signals on the map, crossing variables, or anything from the organic code (`junction_organic.*` is not in this branch's scope).
- Do not touch savegame, network or command code.
- Do not use `std::to_string` or `std::printf` in tests (poisoned); use `fmt::format` or `std::cout`.
- Do not use `pkill`, broad `kill` or bare `git stash`.

## Stop conditions
- Any of the 8 layouts differs and you cannot find a translation mistake: stop, report the first difference (tile, expected, got) and the edge it came from.
- A decision in the old code cannot be expressed with the types above (for example a window that depends on an earlier route in a way that is not a difference constraint along x): stop and report the code line and what it needs.
- `[junctionmap]` takes more than 5 s.

## Gates
- `openttd_test` builds with 0 warnings.
- `[junctionmap]` green, under 5 s.
- Once at the end: `"[junction]~[slow]"` to a file, all green; report its total time. Then `"[junction][slow]"` once, all green; report its total time.
- One commit on `wt-m1`: `Feature: Junction template expressed as a map and projected`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short, this format)
- References read
- Commit: hash and subject
- Red evidence per new test
- Types and members added or removed: names only
- Suite results with times (`[junctionmap]`, `[junction]~[slow]`, `[junction][slow]`)
- BENCH lines
- Stops and anything left undone
- State: `git status --porcelain` and `git log -1`

When you have reported, stop.
