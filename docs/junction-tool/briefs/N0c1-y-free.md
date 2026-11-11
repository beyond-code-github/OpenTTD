# N0c1: vertical runs are free, depth is a variable (DESIGN-N3 §5; D185a, D189)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
Worktree `/home/pete/OpenTTD/.claude/n0c1`, branch `wt-n0c1`, head given at launch (feature/junction-tool). Create `build` (Debug) and `build-rel` (Release) (clang-18, ccache); build `openttd_test` only. Tests only through `tools/junction/jt`: `'[nodes]'` (budget 1 s Release, 5 s Debug); `jt gate build-rel` once before the commit. Output in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/n0c1/`.

## Read first (once each, offset/limit)
`CODINGSTYLE.md`; `docs/junction-tool/DESIGN-N3.md` (whole, short; §5 is this step); `src/junction_nodes.h`; `src/junction_nodes.cpp` lines 1-640 and 815-845 (NodeAnchorShift); `src/junction_facts.h` lines 20-50; `src/tests/junction_nodes.cpp` lines 60-160 and 240-370.

## Model (settled; DESIGN-N3 §5)
X runs and Y runs are both free links. Two class systems in one JunctionPositions: X classes (union over rigid links and Y-free links) are variables [0, nx); Y classes (union over rigid links and X-free links) are variables [nx, nx + ny). Everything below is the X rule of N3a/N3b written for both axes.

## Changes to `src/junction_nodes.h`
```cpp
struct JunctionNodePoint {
	TileIndexDiffC tile; ///< In the flat layout.
	Trackdir trackdir;
	uint16_t cls; ///< Its x variable (an X class, < x_classes); x = value(cls) + tile.x - flat[cls].
	uint16_t ycls; ///< Its y variable (a Y class, >= x_classes); y = value(ycls) + tile.y - flat[ycls].
};
struct JunctionNodeLink {
	uint32_t from;
	uint32_t to;
	bool free; ///< No on_bridge piece from `from` to `to` inclusive, and either same row with X trackdirs or same column with Y trackdirs: its length may change.
	Axis axis; ///< A free link's axis (Axis::X for rigid links).
};
struct JunctionNodePath { ... int8_t dir; ///< x direction of the free X links ... ; int8_t dir_y; ///< +1 (y grows) or -1: the y direction of the path's free Y links; 0 when it has none. };
struct JunctionNodePiece { TileIndexDiffC tile; Track track; bool on_bridge; uint16_t cls; uint16_t ycls; };
/** A flat bridge with the variables of its heads. */
struct JunctionNodeBridge {
	JunctionBridge bridge;
	uint16_t cls;
	uint16_t ycls;
};
struct JunctionNodeTemplate {
	... std::vector<JunctionNodeBridge> bridges; ///< replaces the pair vector
	std::vector<int> flat; ///< Per variable, its flat value: X classes (the flat x of the class's first point) then Y classes (the flat y of the class's first point).
	uint16_t x_classes; ///< The number of X classes; Y classes follow.
	uint16_t anchor; ///< unchanged meaning (X).
	uint16_t anchor_y; ///< The Y class of the first point of EIn->SOut (on A); UINT16_MAX when anchor is.
	...
};
```
Update every doc comment that says "class" for x only. Keep every other declaration.

## Changes to `src/junction_nodes.cpp`
1. ReadPoints: free per the new JunctionNodeLink rule (Y trackdirs: Trackdir::Y_SE, Trackdir::Y_NW); set `axis`; `dir` from X-free links and `dir_y` from Y-free links, each with its own "path <i> has free links both ways" error (add " along y" for Y). Two union-finds: X joins rigid and Y-free links, Y joins rigid and X-free links. Number X classes by first point (as now), then Y classes by first point, offset by nx; `flat` = X flats then Y flats; `x_classes` = nx. Pieces strictly inside any free link are excluded (link_of covers both axes); pieces and bridges get both variables; a bridge's heads must agree on both ("heads in different classes").
2. NodeBuilder: generalise `NodeX` to a coordinate with its variable and use it for both axes (AddX works unchanged over variable indices; add the y counterpart of At()).
3. AddPathDiffs: rule 1 per free link along its axis with that axis's direction. add_gap(p, q, g): sx = cls differ, sy = ycls differ; neither: constant check as now; only X: the existing formula with dir and x; only Y: the same formula with dir_y and y; both: Fail("path <i> points <p>,<q>: gap spans free links on both axes").
4. AddCrossings (DESIGN-N3 §5): for a bridge along axis A (X when its heads share y) at line c of axis B, for each piece of another path on line c strictly between the heads along A:
   - inside a free link along A: Fail as now ("has a free link under it");
   - inside a free link along B: that link's ends straddle c along B: dirB * (c - from) >= 1 and dirB * (to - c) >= 1 over the B variables (today's Y-bridge code, generalised);
   - otherwise (rigid): strictly between the heads along A (>= 1 each) AND equal to the bridge's line along B (two diffs, k 0) — for an X bridge the second part is new (rows), for a Y bridge the first part is new (y between the heads).
5. AddBoxEdges: as now on x, and the same on y (ends at the min/max flat y share y).
6. Anchor: `anchor_y` = ycls of the first point of EIn->SOut. AddJunctionNodeConstraints fixes anchor and anchor_y to their flat values (each only when not UINT16_MAX).
7. RenderJunctionNodeTracks: pieces at (tile.x + shift(cls), tile.y + shift(ycls)); X-free links fill Track::X along the row as now (row moved by shift(ycls)); Y-free links fill Track::Y along the column (moved by shift(cls)); bridges move by both shifts.
8. SolveJunctionNodes: aux variables only for X-free links (positions size = flat.size() + 3 * X-free links). Order:
   (2) constraints; (2b) NEW: snap every Y variable (index order, nearest to flat, the step-5 loop) with no facts; (3) per X class, the existing domain check with each piece's working row = tile.y + value(ycls) - flat[ycls] + offset.y, plus, for every Y-free link whose points have this X class: every interior row r (strictly between the two points' snapped working rows) must have facts.Straight(column, r, Axis::Y), and consecutive interior rows r, r+1 (in +y order) need exit(r) == entry(r+1); failure -> Piece with that class; (4) X-free links as now with the snapped row; (5) snap X variables; (6) render. `values` holds every variable in index order. flat_bits (step 3) fills Track::Y for Y-free interiors.
9. JunctionFacts: add `int MinY() const` / `int MaxY() const` beside MinX/MaxX (origin_y, height), only if step 8 needs them (rows outside the snapshot must fail Straight; if Straight already returns false there, do not add them).
10. NodeAnchorShift: x from the last piece of EIn->SOut (as now), y from its first piece (on A, fixed by anchor_y).
11. InsertJunctionDetour and SearchJunctionNodes: act only on links with axis X (failures name only X links). Everything else unchanged.

## Tests, red first (`src/tests/junction_nodes.cpp`, `[junction][nodes]`). Red: the members exist, the builder puts every point in ONE Y variable and makes no Y-free links; record each failure.
1. (outer) "a deeper box keeps the flat T clean": FlatT, tpl, positions over tpl.flat.size(), AddJunctionNodeConstraints; S = the last point of EIn->SOut; add y_S - y_A >= (flat y_S - flat y_A) + 2 (A = the first point of EIn->SOut, written over ycls with the offsets as test 2 does for x); REQUIRE true; snap, RenderWithSignals, CHECK CheckJunctionRules empty (INFO ViolationText), CHECK layout.size.y == flat.size.y + 2.
2. "the flat T's vertical runs are free" (inner): REQUIRE tpl.error.empty(); at least 3 links with free && axis == Axis::Y; for each, both points share cls and tile.x; anchor_y == ycls of the first point of EIn->SOut. WARN the counts: points, links, X-free, Y-free, X classes, Y classes, diffs.
3. "a house on a vertical leg fails on its class" (inner, facts): MakeMapTestTerrain; block the working tile at the middle interior row of EIn->SOut's Y-free link (map the flat tile with FlatToWorking); SolveJunctionNodes; CHECK no layout, failure kind Piece, cls == the X class of that link's points (the anchor class, which cannot move).
4. (measurement, WARN only, no CHECK) "a longer E->S vertical leg": as test 1 but add y_q - y_p >= flat + 1 for EIn->SOut's Y-free link's points p, q (no deeper box constraint); WARN whether AddDifference and the snap succeed, and if so the rule violations of the render.
All existing [nodes] tests stay as they are and must pass.

## Do not
No change to junction_positions, junction_chain, the generator, the router or the checker. No facts on Y variables (no leg lengthening for terrain). No detours along Y. No `std::to_string`/`std::printf`; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
- Any existing [nodes] test fails after the change: report which and the first failing CHECK verbatim; do not adjust its expectations.
- The builder errors on the flat T (report verbatim, e.g. "gap spans free links on both axes": that is a measurement of the model).
- Test 1 fails on CheckJunctionRules: report violations verbatim.
- OVER BUDGET; anything not covered.

## Gates
0 warnings; `jt fast build '[nodes]' 5`, `jt fast build-rel '[nodes]' 1`, `jt gate build-rel`. One commit on `wt-n0c1`: `Feature: Junction node template with free vertical runs (DESIGN-N3 §5)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence per test; members added; test 2's counts; test 4's measurement; times of the house/hub/flat searches (from the existing WARNs); `[nodes]` times; gate; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
