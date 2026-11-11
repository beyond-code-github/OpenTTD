# G1c(ii-b): render a seed placement to a track-only layout (DESIGN-G1 §11.10)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (persistent; Debug `build`, Release `build-rel`). First: `git checkout -B wt-g1c2b <HEAD>` (given at launch). You are the only writer there. `tools/junction/jt` builds openttd_test before running.
- PlaceJunctionTrackGraph (G1c(ii-a)) and CutJunctionTrackBridges (SB1b, series bridges) gives per edge and token a span of tiles, node tiles, port tiles and bridge heads. This step draws it: one pure function, a track-only JunctionLayout. Nothing in the game calls it yet.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); docs/junction-tool/TESTS.md (the `[topology]` row).
- docs/junction-tool/DESIGN-G1.md §10.7, §11.8, §11.9, §11.10 (the end of the file).
- src/junction_topology.h :150-end (structure, steps, programs, placement).
- src/junction_topology.cpp: PlaceJunctionTrackGraph from its start to the end of the `bend_tiles` lambda, and its last part from "The heads of a bridge" to its end (token spans, bridges).
- src/junction_route.h (whole) and src/junction_route.cpp RasterizeJunctionRoute, AppendPiece, AddRasterToLayout.
- src/junction_layout.h :20-70 (JunctionTrack, JunctionBridge, JunctionLayout).
- src/tests/junction_topology.cpp: PlacementOf and the placement tests; src/tests/junction_nodes.cpp :35-60 (FlatT, MakeTestJunctionSpec use) and :1461-1476.
- src/junction_nodes.h :100-130 (JunctionNodeTemplate fields).

## Design (settled)
### Shared bend moves
In junction_topology.cpp, lift the move-string part of `bend_tiles` into a file-local function above PlaceJunctionTrackGraph, and make `bend_tiles` call it (no behaviour change):
```cpp
/** The moves of a bend from its axis piece: the first turn, the halves after the first, the closing turn (DESIGN-G1 §11.8). A hand of +1 is a clockwise turn. */
static std::string JunctionBendMoves(int hand, int halves, bool corner)
```
Also lift the `axis[4]` trackdir table (headings 0 N, 1 E, 2 S, 3 W) to a file-local `static constexpr Trackdir JUNCTION_AXIS_TRACKDIR[4]` used by both.

### The function
In junction_topology.h after PlaceJunctionTrackGraph:
```cpp
/**
 * The track-only layout of a seed placement (DESIGN-G1 §11.10): one route per lane path and copy, rasterized and merged.
 * @param error Receives the reason when the result is empty (no tracks).
 */
JunctionLayout RenderJunctionTrackPlacement(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackStructure &structure, const JunctionTrackPrograms &programs, const JunctionTrackPlacement &placement, std::string *error);
```
Implement after PlaceJunctionTrackGraph. On any failure: `*error` set, return an empty JunctionLayout{}. layout.size = placement.size.

For each path p (paths order), for copy c in {0} or {0, 1} when one of its edges is a doubled leg (a structure bridge with that edge and doubled = true), build a JunctionRoute:
1. Heading h from the in-port group's side (W: 1 east, E: 3 west, S: 0 north; N side: refuse, "an N port is not ruled yet"). start = placement.ports[in slot], start_trackdir = JUNCTION_AXIS_TRACKDIR[h]. Keep `cur` = the current axis tile (start) and the move string.
2. For each edge e of graph.path_edges[p], its tokens and spans: copy 1 tokens/spans (programs.copies[e], placement.copies[e]) when c == 1 and e is the doubled leg, else programs.edges[e], placement.edges[e]. For token t with span (first, last):
   - **Fan, Corner, SBend:** let D be the unit step of h. Search halves 1..64 and hand in {token hand} (both -1 and +1 for a Fan): rasterize `{ {0,0}, JUNCTION_AXIS_TRACKDIR[h], JunctionBendMoves(hand, halves, kind == Corner) }`; with o1 = pieces[1].tile - pieces[0].tile and oL = pieces[halves].tile - pieces[0].tile, the bend fits when last - first == oL - o1. Exactly one (hand, halves) must fit, else fail "path {p} copy {c} edge {e} token {t}: {n} bend matches". A Fan with first == last == cur, or whose span lies on cur's line with no sideways offset, is skipped. The axis tile before the bend is a = first - o1; F count = the steps from cur to a along D (must be an exact non-negative multiple of D, else fail "... negative or off-line run"); append that many 'F', then the bend moves; then cur = a + (pieces.back().tile - pieces[0].tile) and h = the heading of pieces.back().trackdir (Corner changes it, SBend and Fan keep it).
   - **Over:** b = the structure bridge with edge e whose crossed holds token.index. If b is the same as the previous Over's bridge in this route, nothing. Else: heads = placement.bridges[b][e is a doubled leg ? c : 0]; near = heads.start (the head reached first); F count from cur to near - D (as above); append 'F's, then "B" + std::to_string(middle) where middle = steps between the heads - 1; cur = heads.end.
   - **Under:** nothing.
3. After the last edge: F count from cur to the out-port tile placement.ports[out slot]; append.
4. RasterizeJunctionRoute; a raster error fails "path {p} copy {c}: {raster.error}". The last piece's tile must equal the out-port tile, else fail "path {p} copy {c} misses its out port". AddRasterToLayout(layout, raster).

If any step above does not fit what you find (e.g. the axis piece before a bend is not the tile the rasterizer starts from, or a bridge after an S-bend needs D110's head-from-half-piece form), STOP and report the path, token and tiles; do not adapt the rule.

## Tests (tag `[topology]`; record each red)
Add a helper `RenderOf(topology, nmax, &error)` next to CutOf (SB1b): the same pipeline through CutJunctionTrackBridges, then RenderJunctionTrackPlacement with the cut structure and its placement. Every render test uses the cut structure (series bridges), never the uncut one.
1. **T template oracle (outer)** "topology: the T render traces like the flat T": BuildJunctionNodeTemplate(render, MakeTestJunctionSpec()) has an empty error and the same sizes as the template of the traced flat T (FlatT() in junction_nodes tests: make the fixture reachable, e.g. move the needed declaration or reuse the same generator call; if that needs more than a declaration, STOP and report): points, links, paths, pieces, bridges, joins, diagonals, and x_classes. Red: the function does not exist (a link error counts here only as the first red; record the first assertion failure after a stub that returns an empty layout).
2. **Switches only at nodes** (both topologies, at nmax 64; one TEST_CASE each or a loop): for every layout tile whose TrackBits hold two tracks sharing a tile edge, the tile is in placement.nodes, or is a doubled leg's lane split (placement.copies[leg] SBend span first) or lane merge (placement.edges[leg] SBend span last). Red with the stub: no tracks (assert the layout is non-empty first).
3. **No level crossing, no track on a head:** no tile holds both TRACK_BIT_X and TRACK_BIT_Y; no layout track tile equals a bridge head tile.
4. **Bridges as placed:** layout.bridges, sorted, equals the placement's heads (copy 0 of every bridge, copy 1 of every doubled bridge), each with start/end ordered by smaller x + y first, sorted.
5. **Inside the box:** every track offset is within [0, size).
6. **Every path is drawn:** for each path and copy, the in-port and out-port tiles hold track (a check on the layout, cheap).

## Do not
- Do not change PlaceJunctionTrackGraph's behaviour (only the two lifts named above); do not change its tests' numbers.
- Do not add signals, stations, terrain or a template call to the render function.
- Do not draw an edge on its own (paths only); do not deduplicate routes yourself (AddRasterToLayout merges).
- Do not touch junction_nodes.cpp, the checker, or the generator.

## Stop conditions
- A bend with no match or several matches, an off-line run, a raster error, a switch off a node, a level crossing: report which path/token/tile, with RenderJunctionAscii of the layout drawn so far written to a file you name. These are design findings (band slices, side tracks: DESIGN-G1 §11.10), not things to fix in placement.
- Test 1's counts differ: report both count lists; do not change them.
- Anything not covered here.

## Gates
- `tools/junction/jt fast .claude/nodes/build '[topology]' 5` green; then `tools/junction/jt gate .claude/nodes/build-rel` green (report the count).
- One commit: `Add: Junction seed render (DESIGN-G1 §11.10)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. TESTS.md `[topology]` row: add "G1c(ii-b)" and the new test count.

## Report
The standard report, plus RenderJunctionAscii of the T and of LL_RR (at nmax 64) written to docs/junction-tool/wip/render-T.txt and render-LLRR.txt (not committed; name the paths). When you have reported, stop.
