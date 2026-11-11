# N0c2: free diagonals and the two-phase solve (DESIGN-N3 §6; D185a, D189, D193)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
Worktree `/home/pete/OpenTTD/.claude/n0c2`, branch `wt-n0c2`, head given at launch. Create `build` (Debug) and `build-rel` (Release) (clang-18, ccache); build `openttd_test` only. Tests only through `tools/junction/jt`: `'[nodes]'` (budget 1 s Release, 5 s Debug); `jt gate build-rel` once before the commit. Output in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/n0c2/`.

## Read first (once each, offset/limit)
`CODINGSTYLE.md` (read it: the last step skipped it); `docs/junction-tool/DESIGN-N3.md` §5-§6; `docs/junction-tool/RECON-turn-programs.md` §1-§2 (the paths and diagonals D1-D11 of the flat T); `src/junction_nodes.h`; `src/junction_nodes.cpp` (whole: ReadPoints, AddPathDiffs, AddCrossings, AddBoxEdges, JunctionNodeSnap, RenderJunctionNodeTracks, SolveJunctionNodes); `src/junction_route.h` lines 15-60 (JunctionRoute, RasterizeJunctionRoute); `src/console_cmds.cpp` lines 335-360 (how `train=hp,N,t,drag,speed` becomes JunctionTrainParams) and the spec builder it then calls (grep from there); `src/tests/junction_nodes.cpp` (helpers and the N0c1 tests at the end).

## Header changes (`src/junction_nodes.h`)
```cpp
struct JunctionNodeLink { ...existing...; bool diagonal; ///< A free diagonal (DESIGN-N3 §6); then `free` is true and `axis` is Axis::X. };
/** A free diagonal: its length changes by two half pieces per map step (DESIGN-N3 §6). */
struct JunctionNodeDiagonal {
	uint32_t link; ///< Index into links.
	int8_t dx; ///< The x of one map step along it (two half pieces).
	int8_t dy; ///< The y of one map step along it.
	uint16_t halves; ///< Flat half pieces from the link's `from` to its `to`, both included.
};
/** A unit rule between two points of a path with a free X run between them: an X difference added after the Y snap (DESIGN-N3 §6). */
struct JunctionNodeLateGap {
	uint32_t path;
	uint32_t p; ///< Point before q on the path.
	uint32_t q;
	int g; ///< Required unit gap.
	int u; ///< Flat unit gap.
	std::vector<uint32_t> diagonals; ///< Indices into JunctionNodeTemplate::diagonals between p and q.
};
/** An X-axis bridge over the interior of a free diagonal (DESIGN-N3 §6). */
struct JunctionNodeLateCrossing {
	uint32_t bridge; ///< Index into bridges.
	uint32_t diagonal; ///< Index into diagonals.
};
struct JunctionNodePath { ...; int8_t dir; ///< x sign of its X runs and free diagonals; int8_t dir_y; ///< y sign of its Y runs and free diagonals; };
struct JunctionNodeTemplate { ...; std::vector<JunctionNodeDiagonal> diagonals; std::vector<JunctionNodeLateGap> late_gaps; std::vector<JunctionNodeLateCrossing> late_crossings; };
/** Add the constraints that need the Y values (DESIGN-N3 §6 phase 3): free diagonals' x, late gaps, late crossings. values holds every Y variable's fixed value (X entries ignored); false when infeasible. */
bool AddJunctionNodeLateConstraints(const JunctionNodeTemplate &tpl, JunctionPositions &positions, const std::vector<int> &values);
```
JunctionNodeSnap becomes: snap the Y variables (index order, nearest flat value in [Lo, Up], as now), then AddJunctionNodeLateConstraints (false -> return {}), then snap the X variables; returns all values in index order. Update its doc comment.

## Builder (`BuildJunctionNodeTemplate`)
1. Free diagonal links (DESIGN-N3 §6): consecutive points p, q of a path that are the first and last piece of one run whose trackdirs are half pieces (not X/Y), with at least 3 pieces from p to q inclusive, no on_bridge piece, and no piece lying strictly between the heads of a Y-axis bridge on that bridge's column. Set free = true, diagonal = true, axis = Axis::X; record a JunctionNodeDiagonal with (dx, dy) = TileIndexDiffCByDir(TrackdirToDirection(p's trackdir)) and the flat half count. Its interior pieces are excluded from `pieces` (link_of, as free runs). It joins no class in either union-find.
2. Monotone: dir from X runs and free diagonals (dx), dir_y from Y runs and free diagonals (dy); mixed signs -> error ("path <i> moves both ways along x" / "along y"). Rule 1 for a free diagonal: m >= m_min with m_min = -((halves - 2) / 2) (integer division), written as dy * (y_q - y_p) >= dy * (flat y_q - flat y_p) + m_min over the Y variables.
3. add_gap(p, q, g) per path: let hasX = an X run lies between p and q on the path, hasY = a Y run or a free diagonal lies between. Neither: constant check as now. hasY and not hasX: Y difference d_y * (y_q - y_p) >= d_y * Fy + ceil((g - U) / 2). hasX: push a JunctionNodeLateGap (no static diff). Remove the "gap spans free links on both axes" error.
4. Crossings: an X-axis bridge with a piece of another path strictly inside a free diagonal: push a JunctionNodeLateCrossing (once per bridge and diagonal) and add the static Y side: the bridge row within the diagonal's rows, inclusive (dy * (row - y_p) >= 0 and dy * (y_q - row) >= 0 over the Y variables). Every other crossing case as now.

## AddJunctionNodeLateConstraints
With y values known: for a diagonal i, m_i = dy * ((y_q - y_p) - (flat y_q - flat y_p)) and halves_i = halves + 2 m_i.
- Each free diagonal: x_q - x_p = (flat x_q - flat x_p) + dx * m_i (two differences).
- Each late gap: Δy = (y_q - y_p) - Fy; Σm over its diagonals; add d_x * (x_q - x_p) >= d_x * Fx + ceil((g - U - 2 * d_y * Δy + 2 * Σm) / 2) (d_x = the path's dir; d_y = dir_y, 0 if none).
- Each late crossing: raster the diagonal from p (JunctionRoute at p's flat tile and trackdir, program of halves_i - 1 'F' steps); the raster pieces strictly between the first and last whose y offset from p equals (bridge row - y_p): for each, x offset o: (x_p + o) - h1 >= 1 and h2 - (x_p + o) >= 1 over the X variables (h1, h2 the heads' x).

## Render and solve
- RenderJunctionNodeTracks: a free diagonal's interior = the raster from p at its current length (m from the values), pieces strictly between the first and last.
- SolveJunctionNodes: aux variables for free X runs only (not diagonals). Order: (2) AddJunctionNodeConstraints; (2b) Y snap; (2c) NEW: AddJunctionNodeLateConstraints, false -> failure Template; (3) X class domains as now, plus: for every free diagonal, every interior raster tile (at its current length) must fit a half piece (JunctionNodeTrackFits with that piece's track bit), a restriction on p's X class; (4) free X runs as now; (5) X snap; (6) render.
- InsertJunctionDetour and SearchJunctionNodes: unchanged; they act on free X runs only (exclude diagonal links where they pick free links).

## Tests, red first (`src/tests/junction_nodes.cpp`, `[junction][nodes]`). Red: members present, no diagonal made free; record each failure.
1. (outer) "the node solve builds the T at 100 to 196 km/h on flat land": for speed in {100, 120, 140, 160, 180, 196}: spec = the spec junction_export builds for `ml 1 branch 1 tl 4 train=14194,1090000,561,25,<speed>` without prio= (copy the console path's construction exactly; if it needs game state the test cannot provide, STOP and report). tpl = BuildJunctionNodeTemplate(FlatT layout, spec) (the test-spec T is the topology source); REQUIRE tpl.error.empty(); terrain MakeMapTestTerrain(spec); offset as FlatToWorking but with MakeJunctionFrame(spec); SolveJunctionNodes; REQUIRE layout (INFO FailureText); CHECK(CheckJunctionRules(layout, spec).empty()) (INFO ViolationText); WARN speed, prio_tiles, the halves of every free diagonal, layout size. (Expected red: template errors "gap 5 below 7" at 180/196; record which speeds fail before.)
2. "a lower N split stretches N->W's diagonal" (inner): FlatT at the test spec; positions; AddJunctionNodeConstraints; add y_split - y_A >= flat + 1 for the N split point (the shared split of NIn->EOut and NIn->WOut) and A = the first point of EIn->SOut; JunctionNodeSnap; render with signals; CHECK rules clean; CHECK the NIn->WOut free diagonal's halves = flat + 2 (from the values).
3. "the flat T's free diagonals" (inner): exactly 5 diagonal links (RECON D1, D3, D4, D5, D7: halves 5, 5, 6, 5, 13); D2 (EIn->SOut's second diagonal, under N->E's bridge) is rigid.
All existing [nodes] tests stay as they are and pass (N0c1's test 4 is a WARN measurement: report its new result).

## Do not
No change to junction_positions, junction_chain, the generator, the router, the checker, the signal pass. No free 2-half S-bends. No order/collision constraints between unrelated tracks. No `std::to_string`/`std::printf`; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
- Test 1 fails CheckJunctionRules at some speed: report per speed the violations verbatim and the diagonals' halves (a measurement: the next missing template rule under D193). Commit the rest with that speed's CHECK turned into a WARN "known gap: <rule>" ONLY if the other speeds pass; otherwise stop uncommitted and report.
- Any existing test fails: report the first failing CHECK verbatim; do not change expectations.
- A builder error on the flat T; OVER BUDGET; anything not covered.

## Gates
0 warnings; `jt fast build '[nodes]' 5`, `jt fast build-rel '[nodes]' 1`, `jt gate build-rel`. One commit on `wt-n0c2`: `Feature: Junction node model with free diagonals and a two-phase solve (DESIGN-N3 §6)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence; members added; test 1 per speed (ok / violations, halves, size); test 2; N0c1 test 4's new result; `[nodes]` times; gate; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
