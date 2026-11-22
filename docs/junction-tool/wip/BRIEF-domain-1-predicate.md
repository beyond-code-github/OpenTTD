# BRIEF domain-1: the domain predicate (legal track geometry) and its hand cases

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 90 minutes wall in total. Every test in this step runs in well under 1 s; there are no solves. No rerun loops: if a test fails for a reason you cannot name in two looks, stop and report.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>` (given at launch). Commit there; do not merge.
- Plan: `docs/junction-tool/wip/CARD-domain-rules.md` (main checkout `/home/pete/OpenTTD`), design point 1 and step 1. Ruling D428 (`docs/junction-tool/DECISIONS.md`).
- Why: the drawing program's keep-apart rows are an encoding of "is this geometry legal in the game?". This step writes that question down once, from the game's tile rules, independent of every solver. A later step compares our rows against it.

## 2. Read first (once each, with offset/limit)
- `CODINGSTYLE.md` (repo root).
- `src/junction/junction_route.h` (JunctionPiece, JunctionRoute, JunctionRaster, RasterizeJunctionRoute) and the `'B'`/`'T'` case in `src/junction/junction_route.cpp` (about lines 100-170): bridges and tunnels, `on_bridge` pieces.
- `src/junction/junction_track_geometry.h`: `JunctionCornerTrack`, `JunctionTracksMayShareTile`.
- `src/junction/planar/planar_layout.cpp` lines 74-130 (`MakePiece`, `PiecesClash`, `KindOf`) and 1159-1205 (`RasterPlanarLines`): the frame and the existing tile rule. Read for the conventions; do **not** call or include anything from planar_layout.
- `src/tests/junction/planar/planar_node_rules.cpp` (whole file, 279 lines): how hand cases are laid with `RasterizeJunctionRoute`, `AXIS_STARTS`, `DIAGONAL_STARTS`, `CascadeLegs`, trident and merge legs. Your test file follows its style.
- `src/junction/planar/CMakeLists.txt`, `src/tests/junction/planar/CMakeLists.txt`.

## 3. The frame (fixed; use exactly this)
- Points are half tiles, x east, y south. Tile (tx, ty) covers [2tx, 2tx+2] x [2ty, 2ty+2].
- Edge midpoints are the points with x + y odd. Tile (tx, ty)'s edges: `DiagDirection::NE` at x = 2tx (midpoint (2tx, 2ty+1)), `SW` at x = 2tx+2 (midpoint (2tx+2, 2ty+1)), `NW` at y = 2ty (midpoint (2tx+1, 2ty)), `SE` at y = 2ty+2 (midpoint (2tx+1, 2ty+2)). This matches the game: NE is -x, SE is +y.
- Headings are eighths clockwise from north, by unit step: 0 (0,-1), 1 (1,-1), 2 (1,0), 3 (1,1), 4 (0,1), 5 (-1,1), 6 (-1,0), 7 (-1,-1).
- A piece is one track piece between two edge midpoints of one tile: an axis piece is 2 unit steps on an even heading through the tile centre (edges opposite: X for NE/SW, Y for NW/SE); a diagonal piece is 1 unit step on an odd heading (adjacent edges: `JunctionCornerTrack(a, b)`). Its tile is (FloorDiv(from.x + to.x, 4), FloorDiv(from.y + to.y, 4)).

## 4. The tests (write them first; file `src/tests/junction/planar/planar_domain.cpp`, tags `[junction][planar][planardomain]`)

Test helper (in the test file, static): `TrackFromRoute(const JunctionRoute &route, JunctionRaster &raster)` returns the route's points: the first piece's entry midpoint, then each piece's exit midpoint. Exit edge = `TrackdirToExitdir(td)`; entry edge = `ReverseDiagDir(TrackdirToExitdir(ReverseTrackdir(td)))`; midpoint of an edge of the piece's tile as in section 3. It `REQUIRE`s an empty `raster.error`. Every hand case is laid through it, so the cases are in the game's own vocabulary.

### 4a. Outer test: "planar domain: hand cases from the game"
One `SECTION` per case. Each illegal case asserts the **exact set** of break rules (a `std::set<std::string>` of `PlanarDomainBreak::rule`), so it fails for the reason it names; build each illegal case so that only its rule breaks. Each legal case asserts `legal` and no breaks.

Legal:
1. Two parallel X tracks on neighbouring rows (e.g. starts (0,2) and (0,3), `X_SW`, "FFFF").
2. Two diagonal tracks sharing tiles as Upper|Lower (or Left|Right): two parallel diagonal routes one tile row apart whose corners share tiles.
3. A split on an axis trunk and a split on a diagonal trunk: legs laid as in planar_node_rules (`Lay(start, Straight())`, `Lay(start, Turn(side))`), split into domain tracks at the first index where the legs' trackdirs differ (`d`): the trunk is pieces 0..d-1, each leg pieces d..end; one node at the trunk's last point, `in` = {trunk}, `out` = {legs}. Do this for every start in `AXIS_STARTS` and `DIAGONAL_STARTS` and every side that exists (`Sides`), in a loop with `INFO`.
4. A trident (three legs, axis starts).
5. A merge: the split's legs reversed (as planar_node_rules `Reversed`), node `in` = legs, `out` = trunk.
6. A cascade (planar_node_rules `CascadeLegs`): two nodes, split at each `d`.
7. A bridge over a listed crosser: separator `X_SW` with moves containing `B2` (two middles) and straight runs both sides; a `Y_SE` crosser through the first middle's tile, listed in `crossers`. Structure `first`/`last` = the indices of the first and last `on_bridge` pieces of the separator's raster (assert they are the heads: `last - first == 3`).
8. A bridge whose middle shares a tile with a tunnel's middle, nothing under them (tunnels cross by depth, D155: heights decide, not this step). Also two tunnels whose middles share a tile: legal here.

Illegal (expected rule set in braces):
9. `{"diamond"}`: X track from (0,2) `X_SW` "FFFF" and Y track from (2,0) `Y_SE` "FFFF" (they cross on tile (2,2) at grade).
10. `{"connection"}`: track B's last piece is a corner on a tile of track A whose free end is one of A's edge midpoints (A passes through; no node).
11. `{"connection"}`: a split as in case 3 plus a fourth track ending at the node point, not listed in the node.
12. `{"node"}`: a split whose node lists, in `out`, a track whose first point is not the node point (and which touches nothing).
13. `{"grid"}`: points (0,1), (3,1), (4,2) (a bend at a tile centre).
14. `{"turn"}`: points (0,1), (1,0), (2,1) (two corners on one tile, heading 1 then 3).
15. `{"under"}`: case 7 with the crosser not listed (crossers empty).
16. `{"missing crossing"}`: case 7's bridge with a listed crosser that runs parallel to the separator two rows away and never passes under a middle.
17. `{"decks"}`: two perpendicular bridges whose middles share one tile; no heads on each other's tiles; no crossers.
18. `{"head"}`: case 7's bridge with an unlisted Y track through a head tile instead of a middle.
19. `{"span"}`: case 7 with `limit = 1`.

Red: with the function stubbed to return an empty verdict, the illegal sections fail on their rule set; record one failure message. Legal sections pass at red (they guard against over-strictness).

### 4b. Inner test: "planar domain: the rasteriser matches the game's"
For every start in `AXIS_STARTS` and `DIAGONAL_STARTS` (tile (0,0)) and moves in {"FFFF", "LFFF", "RFFF", "FLFRFFF", "FRFLFFF", "FLLFFF", "FRRFFF"}: skip the combinations whose `RasterizeJunctionRoute` reports an error (count the ones kept; `REQUIRE` at least 60). Feed `TrackFromRoute`'s points to `RasterPlanarDomainTrack`; `REQUIRE` an empty error, the same piece count, and per piece the same tile and the same track (`TrackdirToTrack(jp.trackdir)` against the domain piece's `track`).
Red: with the stub returning no pieces, it fails on the count; record the message.

## 5. The changes

New files `src/junction/planar/planar_domain.h` and `.cpp`, added to `src/junction/planar/CMakeLists.txt` (alphabetical, after `planar_components`); the test file added to `src/tests/junction/planar/CMakeLists.txt`. Include only `../../map_type.h`-level basics, `../../track_type.h`, `../../track_func.h`, `../../direction_func.h`, `../junction_track_geometry.h` and std headers; nothing from `planar_*` or any solver.

```cpp
/** @file planar_domain.h The domain: whether a track geometry is legal in the game, independent of every solver (CARD-domain-rules, D428). */

/** One piece of a track: a step between two edge midpoints of one tile (half tiles, x east, y south). */
struct PlanarDomainPiece {
	TileIndexDiffC tile;
	Track track;
	std::array<int, 2> from;
	std::array<int, 2> to;
	int heading; ///< Eighths clockwise from north: 0 (0,-1), 1 (1,-1), 2 (1,0), ... 7 (-1,-1).
};

/** A track's pieces, or why its points are not a legal track. */
struct PlanarDomainRaster {
	std::vector<PlanarDomainPiece> pieces;
	std::string rule; ///< Empty when legal; else "grid" or "turn".
	std::string why;
};

/** A split or merge: the tracks that end at its point and those that start there. */
struct PlanarDomainNode {
	std::array<int, 2> at; ///< An edge midpoint, half tiles.
	std::vector<uint32_t> in; ///< Tracks whose last point is `at`.
	std::vector<uint32_t> out; ///< Tracks whose first point is `at`.
};

/** A bridge or tunnel on its separator: heads at two piece indices of the separator's raster, middles between. */
struct PlanarDomainStructure {
	uint32_t separator;
	uint32_t first; ///< The near head's piece index.
	uint32_t last; ///< The far head's piece index.
	bool tunnel = false;
	int limit = -1; ///< The most middles allowed, -1 for no limit.
	std::vector<uint32_t> crossers; ///< The tracks allowed (and required) under its middles.
};

struct PlanarDomainInput {
	std::vector<std::vector<std::array<int, 2>>> tracks; ///< Per track its points in travel order; consecutive equal points allowed (dropped).
	std::vector<PlanarDomainNode> nodes;
	std::vector<PlanarDomainStructure> structures;
};

/** One way the geometry breaks the game's rules. */
struct PlanarDomainBreak {
	std::string rule; ///< "grid", "turn", "connection", "node", "diamond", "head", "decks", "under", "missing crossing", "span".
	std::vector<uint32_t> tracks; ///< Ascending, each once.
	TileIndexDiffC tile;
	std::string why;
};

struct PlanarDomainVerdict {
	bool legal = true;
	std::vector<PlanarDomainBreak> breaks; ///< Every break, one per (rule, tracks, tile).
};

/** A track's pieces from its points (section 3 frame). */
PlanarDomainRaster RasterPlanarDomainTrack(const std::vector<std::array<int, 2>> &points);

/** Whether the geometry is legal in the game, and every break. */
PlanarDomainVerdict CheckPlanarDomain(const PlanarDomainInput &input);
```

### `RasterPlanarDomainTrack`
Drop consecutive equal points. For each segment: `rule = "grid"` unless dx = 0, dy = 0 or |dx| = |dy|. Walk unit steps from the first point (`rule = "grid"` if it is not an edge midpoint), accumulating steps since the last midpoint; a heading change while steps are accumulated (between midpoints) is `"grid"`. On reaching an edge midpoint, close the piece: one step on an odd heading or two on an even heading, from and to on two different edges of the tile computed in section 3, opposite for axis, adjacent for diagonal; anything else is `"grid"`. A track whose last point is not a midpoint is `"grid"`. Consecutive pieces whose headings differ by more than one eighth (mod 8), or that lie on one tile, are `"turn"`. Stop at the first error; `why` names the point.

### `CheckPlanarDomain`
1. Raster every track. A raster error is one break (its rule, that track, the tile of the last good piece or (0,0)); that track then takes no part in steps 2-6.
2. **Structures, span:** `first < last < pieces.size()`, `last - first >= 2`, every piece first..last on one even heading, and `limit < 0 || last - first - 1 <= limit`; else `"span"` (and the structure takes no part in steps 3-6). Pieces first and last are heads; the pieces between are middles; every other piece of every track is ground.
3. **Connection (rule "connection" / "node"):** collect, per edge midpoint, the ends of ground and head pieces there (middles are off the ground and connect nothing). At a node's point: every end must be the last piece of a track in `in` or the first piece of a track in `out`, else `"connection"` (all tracks with an end there); a listed track whose end is not at the point is `"node"`. Elsewhere: allowed are one end (a track's own first or last point) or exactly two ends that are consecutive pieces k, k+1 of one track; anything else is `"connection"`.
4. **Node turns and the switch:** per node, for each `in` track's last piece and each `out` track's first piece: headings within one eighth and different tiles, else `"turn"`. On a side with two or more tracks, their pieces at the point must all lie on one tile, else `"node"`.
5. **Tiles:** per tile, two ground pieces of different tracks (or non-consecutive pieces of one track) with tracks X and Y: `"diamond"`. A head's tile holding any other piece (any track, any kind, including another structure's middle): `"head"`. Middles of two structures on one tile, both bridges: `"decks"` (a tunnel in the pair: no break, D155). A ground piece on a middle's tile whose track is not in that structure's `crossers`: `"under"`.
6. **Missing crossing:** a crosser with no ground piece on any middle tile of its structure: `"missing crossing"`.
7. Deduplicate per (rule, tracks, tile); `legal = breaks.empty()`.

## 6. Do not
- Do not call, include or copy-adapt `PiecesClash`, `KindOf`, `MakePiece`, `RasterPlanarLines` or anything else from `planar_layout`, `planar_draw` or `planar_slots`: the domain must be independent of the solvers. The rasteriser test against `RasterizeJunctionRoute` is its proof.
- No curve spacing (`JunctionCurveGaps`), no heights, no signals, no ports, no box, no node legs' spacing beyond section 5 step 3-4: they are later steps.
- No adapter from `PlanarDrawResult` or `PlanarShapeVariant`; no enumerator; no change to any existing file other than the two CMakeLists and the one CLAUDE.md line below.
- No new env hooks, no options, no docs beyond the CLAUDE.md line.

## 7. Stop conditions
- A legal hand case (1-8) is reported illegal by the rules of section 5, or an illegal one cannot be built so that only its rule breaks: stop and report the case, its points and the breaks. Do not change a rule to make a case pass.
- The rasteriser disagrees with `RasterizeJunctionRoute` on any kept route: stop and report the first disagreement (start, moves, piece index, both tiles and tracks).
- `on_bridge` pieces turn out not to include the heads, or a bridge's middles are not one piece per tile: stop.
- Anything this brief does not cover.

## 8. Gates
- `tools/junction/jt fast build '[planardomain]' 5` and `tools/junction/jt fast build '[planar]' 5` (Debug, inside budget).
- `tools/junction/jt gate build-rel` (Release).
- Add to `src/junction/planar/CLAUDE.md`, at the end of the file, exactly: `- **Domain** (D428): \`CheckPlanarDomain\` (planar_domain.h) judges a track geometry by the game's tile rules alone (connections only at listed nodes, no X|Y diamond, heads exclusive, deck middles only over listed crossers); it shares no code with any solver; the rasteriser is proven equal to \`RasterizeJunctionRoute\`.`
- One commit: `Add: Junction: the domain predicate, legal track geometry by the game's tile rules (D428)`, ending with the line `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 9. Report (short, this format)
- References read
- Commits: hash and subject
- Red evidence: each new test and the failure message seen before the code existed
- Types and members added: names only
- Suite results (fast tags and gate), naming any failure you did not cause
- Stops and anything left undone, with the reason
- State: `git status --porcelain` and `git log -1`

When you have reported, stop.
