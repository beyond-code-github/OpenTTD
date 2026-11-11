# N3a: the node template of the flat T, and its render (DESIGN-N3 §1-§3)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
Worktree `/home/pete/OpenTTD/.claude/n3a`, branch `wt-n3a`, head given at launch (feature/junction-tool). Create `build` (Debug) and `build-rel` (Release) like the main ones (clang-18 with ccache); build `openttd_test` only. Tests only through `tools/junction/jt` with bracketed tags: `'[nodes]'` (existing; budget 1 s Release, 5 s Debug, TESTS.md); `jt gate build-rel` once before the commit. Output files in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/n3a/`.

## Read first (once each, offset/limit)
`CODINGSTYLE.md`; `docs/junction-tool/DESIGN-N3.md` (whole, short: the model you implement, §1-§3 only); `src/junction_chain.h` and `src/junction_chain.cpp` (ExtractJunctionChains: how paths, pieces, units, curves and switch events are read; your builder reads the same way); `src/junction_positions.h`; `src/junction_layout.h` lines 20-64 (JunctionTrack/Signal/Bridge/Layout); `src/junction_route.h` lines 15-60 (JunctionPiece, NormaliseJunctionLayout); `src/junction_signal_pass.h`; `src/tests/junction_chain.cpp` lines 95-145 (FlatT(), the hand-built S-bend); `src/tests/CMakeLists.txt`.

## Types (new `src/junction_nodes.h` / `.cpp`, registered next to junction_chain in `src/CMakeLists.txt`)
```cpp
/** @file junction_nodes.h The node model's position template read from a flat layout, and its render (DESIGN-N3). */

/** A piece the node model positions (DESIGN-N3 §1): the first or last piece of a run on some path, a switch piece, or the first or last on_bridge piece of a bridge on a path. */
struct JunctionNodePoint {
	TileIndexDiffC tile; ///< In the flat layout.
	Trackdir trackdir;
	uint16_t cls; ///< Its x class; x = value(cls) + tile.x - flat[cls].
};
/** Two consecutive points on a path; deduplicated over paths. */
struct JunctionNodeLink {
	uint32_t from; ///< Point index, first in travel order.
	uint32_t to;
	bool free; ///< Same row, travel along X, no on_bridge piece from `from` to `to` inclusive: its length may change.
};
/** One traced path of the flat layout as points. */
struct JunctionNodePath {
	JunctionPortRole from;
	JunctionPortRole to;
	std::vector<uint32_t> points; ///< In travel order: every piece of the path that is a point.
	std::vector<uint16_t> units; ///< Per point, the flat unit at which its piece starts along this path.
	int8_t dir; ///< +1 or -1: the x direction of travel of the path's free links; 0 when it has none.
};
/** value(a) - value(b) >= k over classes. */
struct JunctionNodeDiff {
	uint16_t a;
	uint16_t b;
	int k;
};
/** A piece of a rigid run (any path piece that is not strictly inside a free link), drawn with its class. */
struct JunctionNodePiece {
	TileIndexDiffC tile; ///< In the flat layout.
	Track track;
	bool on_bridge;
	uint16_t cls;
};
/** The flat layout's positions as classes and difference constraints (DESIGN-N3 §1-§2). */
struct JunctionNodeTemplate {
	std::vector<JunctionNodePoint> points;
	std::vector<JunctionNodeLink> links;
	std::vector<JunctionNodePath> paths; ///< In the checker's path order.
	std::vector<JunctionNodePiece> pieces; ///< Deduplicated by (tile, track).
	std::vector<std::pair<JunctionBridge, uint16_t>> bridges; ///< Every flat bridge with the class of its heads.
	std::vector<JunctionNodeDiff> diffs; ///< DESIGN-N3 §2 rules 1-4.
	std::vector<int> flat; ///< Per class, its flat value: the flat x of the class's first point (lowest point index).
	uint16_t anchor; ///< DESIGN-N3 §2 rule 5.
	std::string error; ///< Empty when the template was built.
};
/** Read the template of a flat layout. */
JunctionNodeTemplate BuildJunctionNodeTemplate(const JunctionLayout &flat, const JunctionSpec &spec);
/** Add the template's diffs and the anchor (fixed to flat[anchor]) to positions over template.flat.size() variables; false when infeasible. */
bool AddJunctionNodeConstraints(const JunctionNodeTemplate &tpl, JunctionPositions &positions);
/** Fix every class in index order to its flat value clamped into [Lo, Up] (difference constraints only: every value in range extends); the resulting values. */
std::vector<int> JunctionNodeSnap(const JunctionNodeTemplate &tpl, JunctionPositions &positions);
/** Draw the paths' track and the bridges at the classes' values (DESIGN-N3 §3) and normalise; no signals and no prio rows. */
JunctionLayout RenderJunctionNodeTracks(const JunctionNodeTemplate &tpl, const std::vector<int> &values);
```

## The rules, exactly
- Paths, pieces and units: as ExtractJunctionChains (MakeJunctionCheckContext with FindJunctionPorts and JunctionTMovements; units = running sum of pieces' units; a curve is at the first piece in the new direction). Switch pieces: SwitchEventsOf(...)'s `second` index.
- Points: see JunctionNodePoint. A piece that is a point on one path is a point wherever another path passes it (identity = tile and trackdir).
- Free links, classes, offsets, dir: DESIGN-N3 §1. Union-find over rigid links. A path with free links in both x directions: set `error` ("path <i> has free links both ways").
- Diffs (DESIGN-N3 §2), each between points p before q on path P with dir d, flat unit difference U, flat x difference F = flat x(q) - flat x(p), required unit gap g: the constraint is d * (x_q - x_p) >= d * F + ceil((g - U) / 2) (ceil rounds toward +infinity, also for negatives). Written over classes with x = value(cls) + tile.x - flat[cls]. When p and q share a class: no diff; if U < g set `error` ("path <i> points <p>,<q>: gap <U> below <g>").
  1. Free link p->q: d * (x_q - x_p) >= 1 (directly, not via units).
  2. Curve gaps: JunctionCurveGaps(hands of P's curves, spec.tl, spec's curve_speed, spec's reference speed: use exactly the arguments JunctionChainViolations passes) -> per gap (i, j, g) the rule above with p, q = the curve points.
  3. Prio coverage: exactly the merges, curves and splits JunctionChainViolations uses: curve c before prio merge m: g = 2 * prio_tiles between c and m; split s before m: g = 2 * prio_tiles + 1.
  4. Crossings: for an X-axis bridge (heads on one row; h1/h2 = its heads ordered by x): every piece of another path on that row with x strictly between them that is in a class: x - x_h1 >= 1 and x_h2 - x >= 1. A piece of another path strictly inside a free link there: set `error`. For a Y-axis bridge (heads on one column c, rows between them): every piece of another path on column c with y strictly between the heads: if it is in a class, its x equals the heads' x (two diffs, k = 0 each side); if it is strictly inside a free link p->q of path Q with dir d: d * (x_bridge - x_p) >= 1 and d * (x_q - x_bridge) >= 1.
  5. Anchor: the class of the last piece of the EIn->SOut path.
- A bridge's heads that are not pieces of any path, or are in different classes: `error`.
- Render: every JunctionNodePiece that is not on_bridge at (tile.x + values[cls] - flat[cls], tile.y) with its track; every free link's pieces strictly between its points: Track::X on the points' row; every bridge translated by values[cls] - flat[cls] in x; tracks merged per tile (TrackBits union); then NormaliseJunctionLayout.

## Tests, red first (new `src/tests/junction_nodes.cpp`, tags `[junction][nodes]`, registered in `src/tests/CMakeLists.txt`; a static FlatT() like junction_chain.cpp's)
Outermost first. Red = the functions exist as stubs returning an empty template / empty layout / false; record each failure message.
1. "a flat T renders back from its template": tpl = BuildJunctionNodeTemplate(flat, spec), REQUIRE(tpl.error.empty()); layout = RenderJunctionNodeTracks(tpl, tpl.flat); then PlaceJunctionSignals(layout, spec); NormaliseJunctionLayout(layout). CHECK the sorted (x, y, bits) tracks, sorted signals (x, y, facing, type, variant) and sorted bridges (start, end, tunnel) equal the flat layout's.
2. "a longer main line between split and merge is still clean": positions over tpl.flat.size() vars, range [-1024, 1024]; REQUIRE(AddJunctionNodeConstraints); on the EIn->EOut path take the split point S and the prio merge point M (its switch points) and add, via AddDifference, d * (x_M - x_S) >= d * F + 2 (F their flat x difference); REQUIRE true; values = JunctionNodeSnap; layout = render, PlaceJunctionSignals, Normalise; CHECK(CheckJunctionRules(layout, spec).empty()) (print each violation with INFO on failure) and CHECK the layout's size.x is the flat's + 2.
3. "the flat T's positions meet its template": every class c: Lo(c) <= flat[c] <= Up(c) after AddJunctionNodeConstraints; and JunctionNodeSnap returns tpl.flat exactly.
4. "the template of a hand-built S-bend" (the layout of junction_chain.cpp's "chains of a hand-built S-bend", built the same way): on the path with two curves, the classes of its points in order are [c0, c1, c1, c1, c1, c2] with c0, c1, c2 distinct, it has exactly 2 free links, dir +1; RenderJunctionNodeTracks(tpl, tpl.flat) has the same tracks as the hand layout.

## Do not
No change to junction_chain, junction_positions, junction_facts, the generator, the router or the checker. No collision, signal-slot or terrain constraints (N3b). No `std::to_string`/`std::printf`; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
- Test 1 fails after the code is complete: stop and report the differing tracks/signals/bridges (first 10 each) and your reading of why. Do not change the render rule to fit.
- Test 2 fails on CheckJunctionRules: report the violations verbatim (they are the template's missing rules, a measurement); commit nothing else to make them pass.
- Any `error` from the builder on the flat T: report it verbatim.
- OVER BUDGET for `[nodes]`; anything not covered.

## Gates
0 warnings; `jt fast build '[nodes]' 5`, `jt fast build-rel '[nodes]' 1`, `jt gate build-rel`. One commit on `wt-n3a`: `Feature: Junction node template of a flat layout and its render (DESIGN-N3)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit hash and subject; red evidence per test; types and members added; the counts for the flat T: points, links (free), classes, diffs per rule 1-4, and how many classes have Lo == flat and Up == flat; `[nodes]` times Debug/Release; gate result; stops; `git status --porcelain` and `git log -1`. When you have reported, stop.
