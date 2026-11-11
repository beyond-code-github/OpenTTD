# N3b: terrain facts in the node solve, no structure changes yet (DESIGN-N3 §4, facts part)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
Worktree `/home/pete/OpenTTD/.claude/n3b`, branch `wt-n3b`, head given at launch (feature/junction-tool). Create `build` (Debug) and `build-rel` (Release) (clang-18, ccache); build `openttd_test` only. Tests only through `tools/junction/jt`: `'[nodes]'` (budget 1 s Release, 5 s Debug); `jt gate build-rel` once before the commit. Output files in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/n3b/`. Another agent may work in `.claude/fx1` on the generator: do not touch its files.

## Read first (once each, offset/limit)
`CODINGSTYLE.md`; `docs/junction-tool/DESIGN-N3.md`; `src/junction_nodes.h` and `src/junction_nodes.cpp` (N3a, which you extend); `src/junction_facts.h`; `src/junction_positions.h`; `src/tests/junction_nodes.cpp`; in `src/tests/junction_map.cpp`: lines 288-320 (MakeMapTestTerrain, BlockMapTestTile, SharedFlatT), 388-391 (FlatWestPortX), 556-590 ("an obstacle on the main line beside the hub gives no layout"), 1020-1048 ("a height change inside the junction's W end moves the shift-back"); `src/junction_generator.h` line 61 (JunctionTMapResult::hub).

## Coordinates
The template is in the flat layout's coordinates. The terrain (snapshot) is in working coordinates: layout (x, y) is working (x + offset.x, y + offset.y) with offset = (frame.hub_x - flat.hub.x, 0 - flat.hub.y), frame = MakeJunctionFrame(spec), flat = the JunctionTMapResult the template was read from.

## Additions to `src/junction_nodes.h` / `.cpp` (nothing else changes in N3a's API)
```cpp
/** Whether track bits fit a working tile by its facts: two or more tracks -> Switch; one X or Y track -> Straight along that axis; one diagonal (half) track -> Half. */
bool JunctionNodeTrackFits(const JunctionFacts &facts, int x, int y, TrackBits bits);

/** What made a node solve infeasible: the first constraint, in the order of SolveJunctionNodes, that could not be added. */
struct JunctionNodeFailure {
	enum class Kind : uint8_t { Template, Piece, Straight, Break };
	Kind kind;
	uint32_t link = UINT32_MAX; ///< Straight/Break: the free link (index into tpl.links).
	int column = 0; ///< Straight/Break: the working x of the bad tile (Break: the tile before the height break, in +x order).
	uint16_t cls = UINT16_MAX; ///< Piece: the class whose pieces fit nowhere left.
};
/** The result of one node solve. */
struct JunctionNodeSolve {
	std::optional<JunctionLayout> layout; ///< Rendered, signalled and normalised (as FinishJunctionCandidate: Normalise, PlaceJunctionSignals, Normalise).
	std::vector<int> values; ///< Per class, when solved.
	std::optional<JunctionNodeFailure> failure;
};
/** Solve the template's positions on the facts (DESIGN-N3 §2 and the facts of §4) and render; offset maps layout to working coordinates. */
JunctionNodeSolve SolveJunctionNodes(const JunctionNodeTemplate &tpl, const JunctionSpec &spec, const JunctionFacts &facts, TileIndexDiffC offset);
```

## SolveJunctionNodes, exactly, in this order
1. JunctionPositions over `tpl.flat.size() + 3 * (number of free links)` variables, range [-1024, 1024]. Aux variables per free link i (in tpl.links order, counting only free links, k = its rank): u = classes + 3k, w = u + 1, w2 = u + 2.
2. AddJunctionNodeConstraints(tpl, positions); false -> failure Template.
3. Per class c in index order: allowed = every value v in [-1024, 1024] such that each JunctionNodePiece of class c that is not on_bridge fits at working (tile.x + v - flat[c] + offset.x, tile.y + offset.y) with JunctionNodeTrackFits, where its bits are the union of the tracks that the N3a render puts on that tile at flat values (so a switch tile is checked as a Switch). Pieces on_bridge (bridge heads): no fact in N3b. RestrictDomain(c, allowed); false (or allowed empty) -> failure Piece with cls c.
4. Per free link (in tpl.links order) p -> q with dir d (the dir of a path that contains it) on layout row r (working row r + offset.y): tie u = x_p + d, w = x_q - d, w2 = x_q - 2d as equalities (AddDifference both ways over the class variables with the offsets x = value(cls) + tile.x - flat[cls]; add them in working x, i.e. include offset.x in the tie constants so u, w, w2 hold working x). For d = +1, the interior is [u, w]; for d = -1, it is [w, u]. Then, for every working column c on that row inside the snapshot's x range, ascending:
   - if `!facts.Straight(c, row, Axis::X, ...)`: AddAvoid over the interior (d = +1: AddAvoid(u, w, c); d = -1: AddAvoid(w, u, c)); false -> failure Straight (link, c).
   - else if c + 1 is in range, Straight there too, and `exit(c) != entry(c + 1)`: a height break between c and c + 1; the interior must not hold both c and c + 1. With w2 = x_q - 2d: d = +1: AddAvoid(u, w2, c) (not u <= c <= w - 1); d = -1: AddAvoid(w2, u, c + 1) (not w + 1 <= c + 1 <= u). False -> failure Break (link, c).
   Also, outside the snapshot's x range, the interior must stay inside it: RestrictRange on u and w to the snapshot's x range.
5. Snap: per class in index order: candidates t in [Lo(c), Up(c)] ordered by |t - flat[c]|, ties lower first; Mark(); RestrictRange(c, t, t); on false Undo(mark) and try the next. (Lo and Up are solutions of a min/max-closed system, so a candidate always succeeds.) values = the classes' fixed values.
6. Render: RenderJunctionNodeTracks(tpl, values); NormaliseJunctionLayout; PlaceJunctionSignals(layout, spec); NormaliseJunctionLayout.

## Tests, red first (in `src/tests/junction_nodes.cpp`, `[junction][nodes]`; copy MakeMapTestTerrain/BlockMapTestTile as static helpers there, unchanged; use a static flat T from ConstructJunctionTMap(MakeTestJunctionSpec(), nullptr, std::nullopt, true) as the existing FlatT())
Outermost first. Red = SolveJunctionNodes stub returns an empty JunctionNodeSolve and JunctionNodeTrackFits returns false; record each message.
1. "the node solve builds the flat T on flat terrain": MakeMapTestTerrain, JunctionFacts, solve; REQUIRE layout; CHECK values == tpl.flat; CHECK the layout equals the flat layout (tracks, signals, bridges, sorted, as N3a's test 1); CHECK every track tile fits (JunctionNodeTrackFits at layout tile + (offset - normalisation shift): compare by mapping the flat layout instead, since the layout equals it).
2. "the node solve moves the W end off an incline": the terrain of junction_map.cpp's "a height change inside the junction's W end moves the shift-back" (same loop, build_on_slopes = true). Solve. REQUIRE layout; CHECK(CheckJunctionRules(layout, spec).empty()) (INFO each violation); CHECK every track tile of the layout fits its working tile (map the layout back: working = layout tile + offset - shift, where shift is how far the render's normalisation moved the layout; derive it from a point of the anchor class, whose working x is fixed). WARN the values that differ from tpl.flat.
3. "a house beside the hub fails on A's run": MakeMapTestTerrain, BlockMapTestTile(terrain, frame.hub_x - 1, 0). Solve. CHECK(!layout); REQUIRE failure; CHECK kind == Straight; CHECK column == frame.hub_x - 1; CHECK the failure's link lies on the EIn->EOut path.
4. "facts move the end of a free run" (inner, synthetic): the hand-built S-bend of N3a's test 4 (no anchor); a JunctionTerrain covering it (origin (0,0), offset (0,0), width 16, height 6, all buildable flat); block the tile on the last run's row two tiles before its flat end. Solve the positions only through SolveJunctionNodes. CHECK the end point's value is the blocked x - 1 and no track of the layout sits on the blocked tile. (PlaceJunctionSignals on a layout without ports: if it throws or asserts, stop and report.)
5. "JunctionNodeTrackFits by kind" (inner): one 4x1 hand terrain: flat; ramp along X; Blocked; flat. CHECK X fits on 0 and 1, Y does not fit on 1 (if Straight Y fails across an X incline; if the facts say otherwise, assert what JunctionFacts says and note it), nothing fits on 2, a half piece and TRACK_BIT_X|TRACK_BIT_UPPER (two tracks) fit on 0 and not on 1.

## Do not
No change to junction_facts, junction_positions, junction_chain, the generator, the router or the checker. No shifts or structure search (N3c). No `std::to_string`/`std::printf`; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
Test 2 fails on CheckJunctionRules or on fits: report violations / unfit tiles verbatim with the values that moved (a measurement; do not add rules to fit). Test 3's failure is a different kind or column: report it. OVER BUDGET. Anything not covered.

## Gates
0 warnings; `jt fast build '[nodes]' 5`, `jt fast build-rel '[nodes]' 1`, `jt gate build-rel`. One commit on `wt-n3b`: `Feature: Junction node solve on terrain facts (DESIGN-N3)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence per test; types and members added; test 2's moved values; `[nodes]` times; gate; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
