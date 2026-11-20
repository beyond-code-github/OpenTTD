# BRIEF G1: Slots' joint solve with no clash rows, and its two oracles (probe, Sonnet)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch wt-slots, head {HEAD}. Debug build dir `build`, Release `build-rel` (both warm, ccache).
- Implements SPEC-slots-lengths §7 G1 with the design in §8 (8.1 lower bounds, 8.2 steps, 8.3 oracles). The spec file is /home/pete/OpenTTD/docs/junction-tool/wip/SPEC-slots-lengths.md (read §3, §7, §8 once at the start).

## Read first (once each, with offset/limit)
- /home/pete/OpenTTD/CODINGSTYLE.md (C++ part).
- src/junction/planar/CLAUDE.md sections "Terms", "Shape layer", "Track map".
- src/tests/junction/planar/planar_shape_layer.cpp lines 295-594: `ClosureLp`, the closure probe. G1's solve copies its structure (points per track and event, parts per piece and stretch, the stretch rows, node ties, crossing ties, port rows, pins, the row tags, IIS reporting). Lines 849-870: how a probe test builds the variants.
- src/junction/planar/planar_shape_layer.h (the variant types); src/junction/planar/planar_shape_layer.cpp lines 120-160 (`ParentChain`) and 560-600 (H8: how a beside track's pieces map to its chain's).
- src/junction/planar/planar_layout.h (`PlanarGeometry`, `PlanarDeck`, `PlanarPiece`, `RasterPlanarLines`); src/junction/planar/planar_layout.cpp lines 95-125 (`PiecesClash`, `KindOf`), 868-915 (the lazy loop's clash scan), 1110-1160 (`RasterPlanarLines`).
- src/junction/planar/planar_model.cpp lines 91-131 (`PieceCurves`), src/junction/planar/planar_components.cpp lines 2858-2868 (`MakeParams`), src/junction/topology/junction_chain.h lines 20-47 (`JunctionCurveGaps`, `JunctionCurveGapsHold`), src/junction/placement/junction_milp.h.

## Commit 1: two seams (src), test-first

### Tests (add to src/tests/junction/planar/planar_shape_layer.cpp, tag `[junction][planar][planarshape]`)
1. "planar geometry clashes: a ground crossing clashes, a deck crossing does not". Build `PlanarTopology t` with `t.lines.resize(2)` (no nodes, no ports needed) and a `PlanarGeometry g`: points (0,1), (3,1), (8,1), (3,-2), (3,1), (3,4); chains {0,1,2} and {3,4,5}; headings {{2,2},{4,4}} (E, S); `node_vertex` empty. Assert `PlanarGeometryClashes(t, g)` has exactly one hit, lines 0 and 1, tile (1,0). Then add `g.decks.push_back({0, 1, 1, 3, 3})` and assert no hit.
2. "planar parent chain: a copy's chain on T2": on the first T2 variant from `BuildPlanarShape(BuildPlanarStructure(topology, spec, 16))`, for every track with `part_of == UINT32_MAX`, `ParentChain` returns true and a non-empty chain.
- Red first: declare both functions, give `PlanarGeometryClashes` a body returning `{}` and leave `ParentChain` as it is but moved (see below); test 1 must fail on its first assertion. Record that failure text. Test 2 is a regression guard on the move (it may pass at once; say so).

### Changes
- src/junction/planar/planar_layout.h: add
  ```cpp
  /** One clash the layout program's tile scan finds between two lines of a geometry. */
  struct PlanarClashHit {
  	uint32_t line_a, segment_a; ///< The first line and its segment (index in its chain).
  	uint32_t line_b, segment_b; ///< The other line (line_b > line_a) and its segment.
  	TileIndexDiffC tile; ///< The first piece's tile.
  	uint8_t rule; ///< The clash rule kind (ground, exclusive, tile).
  };
  std::vector<PlanarClashHit> PlanarGeometryClashes(const PlanarTopology &topology, const PlanarGeometry &geometry);
  ```
  with a doc comment: the layout program's clash scan (family 13) on a given geometry: pieces from `RasterPlanarLines`, every pair of pieces of different lines on one tile or on edge-neighbouring tiles judged by `KindOf` and `PiecesClash`, pairs where both pieces touch the point of a node both lines are incident to left out; one hit per (line, segment) pair.
- src/junction/planar/planar_layout.cpp: define it after the anonymous namespace, written as a copy of lines 873-900's scan (by_tile, at_shared_node, the dx/dy loop, the `pairs` set) producing hits instead of lazy rows. Do not touch the lazy loop.
- src/junction/planar/planar_shape_layer.h: declare `bool ParentChain(const PlanarTrackMap &map, const std::vector<PlanarStructureSite> &sites, uint32_t b, std::vector<uint32_t> &chain);` with its existing doc comment moved from the .cpp; in planar_shape_layer.cpp move the definition out of the anonymous namespace unchanged.
- src/junction/planar/CLAUDE.md: one fact under "Shape layer" (`ParentChain` is public: the parent chain of a beside track, H8) and one under "Layout program" (`PlanarGeometryClashes` runs family 13's scan on a given geometry, the Slots clash oracle).
- Commit: `Codechange: Junction planar: export the clash scan and ParentChain for Slots' oracle`.

## Commit 2: the G1 probe (tests only)

New file src/tests/junction/planar/planar_slots_probe.cpp, registered in src/tests/junction/planar/CMakeLists.txt next to planar_shape_layer.cpp. One static function and one test.

### `static G1Result SolveG1(const PlanarTopology &topology, const JunctionSpec &spec, const PlanarShapeVariant &v)`
`struct G1Result { bool solved; std::string why; double ms; int step_lanes; std::vector<std::string> clashes; size_t same_channel, face, noface; std::vector<std::string> curve_breaches; size_t consistency; size_t no_chain; };` (file-local, in the test file). One `JunctionMilp`, every variable integer, 15 s time limit, every row tagged as `ClosureLp` does. All lengths in half-tile units (spec §8 opening paragraph).

Rule values, built here from `spec` (a stand-in for Structure's hand-down, spec §8.1): `tl = spec.tl`, `cl = DeriveCurveSpacing(spec.reference_speed, spec.curve_speed, spec.tl)`, `g = min(2 tl - 1, max(2, cl))`, `bay = 2 tl + 4`, `coverage = 2 spec.prio_tiles + 4`, `node_leg = 2`, `cross = 1`. Per span: `clear = (diag ? 2 : 1) + 2 * max(end_run[k])` over kinds with `limit[k] >= 0`, `diag` = the span's first crossing has an odd angle (`v.angles`); `lim = min(limit[k])` over the same kinds.

Spans: per structure, (a) the separator, events from `st.crossings.front()` to `st.crossings.back()` in `map.events[st.separator]` inclusive; (b) when `st.copy != UINT32_MAX`, the copy, all its events. The span's track is its separator (the separator or the copy); every other track at those crossings is a crosser. `in_span` = the (track, stretch) pairs strictly between a span's first and last event, as `ClosureLp`'s `in_deck`.

Variables and rows:
1. **Box:** W, H integer >= 2, each `= 2 * aux` (even). Every point 0 <= x <= W, 0 <= y <= H.
2. **Points and parts:** as `ClosureLp` (point per track end and event; per stretch k and piece j of it one part >= 0; stretch rows x and y). In addition each track's start point satisfies x + y odd (`= 2 aux + 1`), and for each axis piece j of each track the cross coordinate of its start point (x for headings 0/4, y for 2/6), written as the track's start point plus the contributions of everything before it in travel order, is odd. Each piece's total of its parts (and step sub-parts) >= 2 for an axis heading, >= 1 for a diagonal.
3. **Nodes, crossings, ports, pins:** as `ClosureLp` with families 1, 2, 4 replaced as follows: crossings tied (same point); ports on their box side; pins exactly as `ClosureLp` bit 4 (d = 2 in one port, `2 * side_gap` between ports); node legs tied to the node point (no cusp variable w: a cusp is a step, item 5).
4. **Step places** (spec §8.2): for each (t, k) not in `in_span`, one place on piece j = the stretch's last piece if stretch k ends at a split node (k is the last stretch and the track's `to` is a node where the track is the in line), the first piece if it starts at a merge node (k = 0 and `from` is a node where the track is the out line), else its first piece. The part of piece j in stretch k becomes two parts, `before` and `after` (each >= 0), with between them integers p, m in [0, 8] and binaries zp, zm: p <= 8 zp, p >= zp, m <= 8 zm, m >= zm, zp + zm <= 1. The stretch rows gain `2 p STEP[h + 1] + 2 m STEP[h - 1]` (h the piece's heading mod 8). Cost 1000 per unit of p and m.
5. **Cusps:** a node leg that is not the site's straight leg and has the trunk's heading (the test `ClosureLp` uses at its line 455) gets its step place on its first piece (split) or last piece (merge), with `before` = 0 for a split (`after` = 0 for a merge), the side fixed to the map's sigma as `ClosureLp` computes it (only p when sigma is +1, only m when -1), and size >= 1. It replaces item 4's place for that stretch.
6. **Beside tracks** (`part_of == UINT32_MAX`): `ParentChain`; on false count `no_chain` and skip. Piece j of the beside track corresponds to piece j of the chain's pieces concatenated (a node's piece once, exactly as H8 concatenates them). For each piece j choose one point on the beside track: an event on piece j if there is one, else the end point of piece j if j is not the last piece, else the start point of piece j; skip the piece only when that point lies on the cusp step (the beside track has a single piece and no event; count it in `no_chain`). The chain's point: the start point of chain piece j, or an event on it. Row: `Dot(beside point - chain point, STEP[(h_j + 2) % 8]) = 2 s`, s = +1 when the beside track is on the right of its parent (from `map.Side` of the beside track's stretch against the parent's, as `ClosureLp` reads sigma), else -1.
7. **Spans:** consecutive events of a span: the span track's parts between them sum >= 2; first to last sum <= 2 (lim - 1 - diag). The span track's sub-part adjacent to its first event, on the event's piece, in the stretch ending there, >= clear; the same after its last event. Two spans on one track whose last and first events sit on one piece with no Shape bend between: everything between them >= clear + clear + 2.
8. **Crossers:** at each span event, the crosser's sub-parts adjacent to the crossing on each side >= 1.
9. **Node legs:** each leg's travel from its node to its first event (its other end if none) >= node_leg.
10. **Rows' coverage:** each track named `prio:...`: its total travel >= coverage.
11. **Bays:** a topology node is one where no incident track has `part_of == UINT32_MAX`. For each route path (`map.expanded.routes[r].paths`) of a route not in the topology's prio list, at each topology merge node on it, the travel from the previous topology split node on the path (or its entry port) to the merge >= bay.
12. **Curves** (per route path, over its tracks concatenated, travel measured as the sum of parts and step diagonals): Shape's bends (every heading change between consecutive pieces, at nodes too) plus cusp steps' two bends are fixed curves; rows as `PieceCurves` over them (j = i + 1: opposite hands >= 2, same hand >= g; j = i + 2, i + 3: >= min(2 tl - 1, (j - i) cl)). For each optional step (item 4), its first bend against the nearest fixed curve before it and its second bend against the nearest fixed curve after it, the j = i + 1 rule with the step's hands (p: first right, second left; m: the reverse), each relaxed by 1000 (1 - zp) or 1000 (1 - zm). No other step rows.
13. **Objective:** 1000 (p + m) summed, plus every part, plus W + H.

After an optimal solve:
- **Consistency:** rebuild every track's vertex list from the solution (start, then in travel order every bend, step bend and event, end) by walking its parts; count in `consistency` each crossing whose two tracks' vertices differ and each node whose incident ends differ.
- **Geometry:** `PlanarGeometry` over `map.expanded`: points and chains from those vertex lists (each track its own vertices), headings per segment, `node_vertex` per expanded node = the vertex of one incident end, decks one per span (`line` the span track, `first`/`last` the chain positions of its first and last event vertices, `back` = `front` = clear), W and H in tiles. Remember per segment its stretch.
- **Clash oracle:** `PlanarGeometryClashes(map.expanded, geometry)`; per hit one line `<trackA> s<stretch> h<heading> x <trackB> s<stretch> h<heading> tile=(x,y) rule=<r> face=<index|none> same_channel=<0|1>`: face = the first face of `map.faces` holding both (track, stretch) walls, same_channel = `BuildPlanarChannels(topology).channel_of_line` equal for `map.tracks[a].line` and `map.tracks[b].line`. Count same_channel, face (not same channel, a common face) and noface.
- **Curve oracle:** per route path, its solved curves in travel order (hand, travel position), `JunctionCurveGaps(hands, spec.tl, spec.curve_speed, spec.reference_speed)`, and each gap not held as `route=<name> path=<i> curves <i>,<j> gap <g> below <min>`.
- On a non-optimal status: `why` = the status plus the unique tags of `InfeasibleRows()`, as `ClosureLp`.

### The test
`TEST_CASE("planar probe: G1 joint slots and lengths on T2, T3 and LL2", "[junction][planar][planarshape][.probe]")`: hubs T2, T3, LL2 (as line 852, without T1); per hub `BuildPlanarShape(BuildPlanarStructure(topology, spec, 16))`, the first 3 variants; per variant print `G1 <hub> <tag> solved=<0|1> ms=<ms> steps=<lanes> W=<tiles> H=<tiles> clashes=<n> same_channel=<n> face=<n> noface=<n> curve=<n> consistency=<n> no_chain=<n> why=<why>`, then each clash as `G1C <hub> <tag> <line>` and each curve breach as `G1V <hub> <tag> <line>`. Per hub a summary `G1 <hub> SUMMARY solved=<n>/3 ms_max=<ms> clashes_total=<n> curve_total=<n>`. No REQUIRE on the outcomes (it is a measurement); REQUIRE only that the topology parses and at least 3 variants shape.
- Commit: `Add: Junction planar probe: G1, Slots' joint solve with the clash and curve oracles`.

## Run
- Debug: `/home/pete/OpenTTD/tools/junction/jt fast /home/pete/OpenTTD/.claude/ng1/build '[planarshape]'` after commit 1 (budget from TESTS.md).
- Release gate after each commit: `/home/pete/OpenTTD/tools/junction/jt gate /home/pete/OpenTTD/.claude/ng1/build-rel`.
- The probe, Release, alone (no other heavy job running): `cd /home/pete/OpenTTD/.claude/ng1/build-rel && timeout 900 ./openttd_test "planar probe: G1 joint slots and lengths on T2, T3 and LL2" > g1-run.txt 2>&1; grep '^G1' g1-run.txt > g1.txt`.

## Do not
- Add clash rows, separation rows, neighbour (frame) rows or ladders of any kind; change `ClosureLp` or any probe in planar_shape_layer.cpp; touch the lazy loop in planar_layout.cpp; read the spec's rule fields anywhere but the stand-in block named above; try variants beyond the first 3; tune any value in §8.1; fix a clash or a curve breach.

## Stop conditions
- `consistency > 0` on any solved variant: the vertex walk or the model is wrong; report the first case and stop.
- A step place, cusp, beside-track mapping or span the rules above do not cover (e.g. a cusp leg whose stretch is in a span): stop and report the case.
- The gate fails in a test you did not touch: rerun that test alone once; if it passes alone, report it as a flake and continue; else stop.

## Report (short)
- References read; commits (hash, subject); red evidence for test 1; types and functions added; gate results (count, time); the `G1 ... SUMMARY` lines and every `G1 <hub> <tag>` line verbatim; the G1C lines grouped by (hub, same_channel/face/noface) with counts and the first 5 of each group verbatim; G1V lines: count per hub and the first 5; stops; `git status --porcelain` and `git log -1`.
When you have reported, stop.
