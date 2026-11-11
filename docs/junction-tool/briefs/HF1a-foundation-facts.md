# HF1a: tile facts per track set from the game's foundation functions

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (persistent; Debug `build`, Release `build-rel`; incremental builds). First: `git checkout -B wt-hf1a <HEAD>` (given at launch). You are the only writer there.
- Decisions: D194 (height facts as designed), D193. Engineer: "the core mechanic still needs to be respected as the game plays". Design: docs/junction-tool/DESIGN-N3.md §10 and its "Revision" bullet; docs/junction-tool/RECON-foundations.md (read both first).
- Why: JunctionFacts decides Switch/Half by "UsableAt at z_max" and Straight by incline, which is looser than the game: on an incline the straight up the slope may not share its tile; low-side diagonals are refused; one- and three-corner slopes have half-tile foundations. The snapshot keeps no exact Slope. This step makes the facts ask the game. The solve changes (joins, Y snap, X filter) are HF1b, NOT this step.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); docs/junction-tool/TESTS.md (jt: tags bracketed).
- src/rail_cmd.cpp :255-408 (tables, GetRailFoundation, CheckRailSlope); src/landscape.h :110-135 and src/landscape.cpp ApplyFoundationToSlope (:182) and GetPartialPixelZ (grep); src/slope_func.h InclinedSlope.
- src/junction_terrain.h (whole); src/junction_game.cpp :250-330 (JunctionCanonicalIncline, the cell fill in MakeJunctionTerrain).
- src/junction_facts.h and .cpp (whole).
- src/junction_nodes.cpp: JunctionNodeTrackFits, NodeInclinedAlong, NodeHeightStep, JunctionNodeHeightsOk (~:1150-1240).
- src/tests/junction_game.cpp: the GameJunctionPricing tests (~:170-210: how a test sets up a map tile with heights) and the near-test helpers; src/tests/junction_facts.cpp (pattern).

## Changes
1. **Snapshot slope.** `JunctionTerrainTile` gains `Slope slope = SLOPE_FLAT; ///< The tile's slope in the canonical frame (corners rotated and mirrored with it, D152), steep bit kept.` Add `Slope JunctionCanonicalSlope(Slope slope, uint8_t rotation, bool mirrored);` declared beside JunctionCanonicalIncline and defined beside it in junction_game.cpp, with the same rotation/mirror convention; MakeJunctionTerrain fills `cell.slope`.
2. **Every test or tool that hand-builds a JunctionTerrainTile with z_max > z_min** (grep `z_max =`, `.incline =`, `steep =` in src/tests and src/*.cpp) also sets `slope` consistently: an inclined tile `InclinedSlope(incline)`; other shapes as the test intends (read the test; if a test's intent is unclear, STOP and ask). List every place you changed in the report.
3. **JunctionFacts**:
   - `bool Fits(int x, int y, TrackBits bits) const; ///< Whether the game can build exactly these tracks on the tile (buildable class as Switch/Half test it today, GetRailFoundation(slope, bits) != Invalid, and a foundation only with build_on_slopes).`
   - `int EdgeZ(int x, int y, TrackBits bits, DiagDirection edge) const; ///< The pixel z of the tile's track surface at the midpoint of the given edge with these tracks built: z_min * TILE_HEIGHT + ApplyPixelFoundationToSlope(GetRailFoundation(slope, bits), s) + GetPartialPixelZ(mx, my, s); INT_MIN when !Fits.` Edge midpoints (pixel x along map X from the NE edge, y along map Y from the NW edge): NE (0, 8), SE (8, 15), SW (15, 8), NW (8, 0). If the game's own convention for these offsets differs (check GetPartialPixelZ and how GetSlopePixelZ is called for a tile), use the game's and say so.
   - Store what they need per cell (slope, z_min, the class test) and `build_on_slopes`. In debug builds assert, per cell, that the slope agrees with z_min/z_max (flat iff equal) and with incline when incline is valid (`InclinedSlope(incline) == slope`, ignoring nothing else); this catches any hand-built tile item 2 missed.
   - Straight/Switch/Half and the run indices stay as they are.
4. **JunctionNodeTrackFits**: one axis track -> Straight as now; otherwise `facts.Fits(x, y, bits)`.
5. **The oracle JunctionNodeHeightsOk** keeps its signature and `why`/doc, and checks the join rule instead of the z walk: build a JunctionFacts from the terrain once; for every traced path, every piece not on_bridge and not a bridge head must Fit with its whole tile's bits (the layout's bits on that tile); for consecutive such pieces A then B: `EdgeZ(A, bitsA, TrackdirToExitdir(tdA)) == EdgeZ(B, bitsB, ReverseDiagDir(TrackdirToExitdir(tdA)))`. Report failures as now ("path i piece j at x,y: ..."). Remove NodeInclinedAlong/NodeHeightStep if nothing else uses them.

## Tests (outermost first; record each red)
1. Outer, `[facts]` in src/tests/junction_game.cpp (it has map setup): `"EdgeZ matches the game's track surface on a real tile"`: for slopes {SLOPE_NE, SLOPE_SW, SLOPE_N, SLOPE_NWS, SLOPE_STEEP_N} and bit sets {X, Y, Y+Upper, X+Lower, Upper, Lower, Left, Right, Upper+Lower}: when GetRailFoundation(slope, bits) is not Invalid, build the tile on a test map with that slope (as the GameJunctionPricing tests set heights) and rail with those bits (MakeRailNormal or the helper those tests use), take a JunctionTerrain of it at rotation 0 (MakeJunctionTerrain or MakeJunctionTerrainRect as those tests do), and for each edge touched by any of the bits: `CHECK(facts.EdgeZ(...) == GetSlopePixelZ(px, py))` at that edge's midpoint in world pixels. Red: does not compile, then (with a stub returning 0) fails. If building such a map tile in a test is not possible with the existing helpers, STOP and report what is missing.
2. `[facts]`: `"Fits follows GetRailFoundation"`: for every non-steep and steep slope and every subset of the six tracks, a one-tile terrain with build_on_slopes true: `Fits == (GetRailFoundation(slope, bits) != Foundation::Invalid)`; with build_on_slopes false: Fits only when the foundation is None. Red with a stub.
3. `[facts]`: `"the canonical slope rotates with the incline"`: for every inclined slope s, rotation 0..3 and mirrored false/true: `JunctionCanonicalSlope(s, r, m) == InclinedSlope(JunctionCanonicalIncline(s, r, m))`. Red with a stub returning s.
4. `[facts]`: `"Straight agrees with EdgeZ for a lone straight"`: for every slope with one axis track alone: `Straight` validity == `Fits`, and when valid `entry * TILE_HEIGHT == EdgeZ(entry edge)` and `exit * TILE_HEIGHT == EdgeZ(exit edge)` (travel along +axis: entry edge NE for X? use the axis' travel direction as Straight documents it). If they disagree on any slope, STOP and report the slopes: Straight is then also wrong and needs a ruling.
5. `[nodes]`: `"the oracle sits a split on an incline level"`: the case HF1 found: an X-inclined tile with the straight across the slope plus a high-side diagonal, reached and left at the top height over flat top-height tiles: JunctionNodeHeightsOk true; and the same tile with the straight up the slope plus a diagonal: false (the game refuses it). Red: the first is false today.
6. Existing `[facts]`, `[nodes]`, `[nodespeed]`, `[tmapnear]`, `[terrainrect]`, `[nodeoracle]` stay green. If an existing test's expectation flips, STOP and report it (do not edit expectations).

## Do not
- Do not touch the solve (SolveJunctionNodes), the search, the template, the line router, or GenerateJunctionNear. Do not add joins. Do not apply docs/junction-tool/hf1-wip.patch.

## Stop conditions
- As listed in the tests. A debug assertion from item 3 firing on a terrain you cannot fix under item 2.
- `[facts]` Release over 1 s; `[nodes]` Release over 1 s.

## Gates
`tools/junction/jt fast build "[facts]" 5`, `tools/junction/jt fast build-rel "[facts]" 1`, `tools/junction/jt fast build "[nodes]" 5`, `tools/junction/jt fast build-rel "[nodes]" 1`, `tools/junction/jt fast build-rel "[tmapnear]" 5`, `tools/junction/jt fast build-rel "[terrainrect]" 5`, `tools/junction/jt gate build-rel` (budget 120 s; note the ORACLE line). Commit: `Feature: Junction tile facts follow the game's rail foundations (D194)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
References read; commits; red evidence per test; types/members/functions added or removed; every hand-built terrain place changed (item 2); suite results with times; ORACLE line; stops; `git status --porcelain` and `git log -1`. When you have reported, stop.
