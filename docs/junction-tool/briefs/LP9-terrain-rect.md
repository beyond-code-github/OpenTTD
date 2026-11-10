# LP9: a map-axis terrain snapshot for painted lines (RECON-linepainter LP9)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/lp9`, branch `wt-lp9`, head given at launch (feature/junction-tool). Create `build` configured like `/home/pete/OpenTTD/build` (Debug); build `openttd_test` only.
- Plan: `docs/junction-tool/RECON-linepainter.md` LP9. The painter routes in map axes (rotation 0, not mirrored): cell (x, y) is map tile TileXY(x, y). Plain rectangle only: no lazy filling, no corridor limiting (simplest first; measured later in LP12).

## Test speed
New tests tagged `[terrainrect]`, under 5 s. Develop against it only; the wider gate once at the end.

## Read first (once each)
`CODINGSTYLE.md`; `src/junction_game.cpp` `MakeJunctionTerrain` (about lines 263-335) and `src/junction_game.h` around its declaration; `src/junction_terrain.h` (`JunctionTerrain`, `JunctionTerrainTile`); `src/tests/junction_game.cpp` the test "MakeJunctionTerrain: heights, steepness and class in the working frame".

## Changes (`src/junction_game.{h,cpp}`)
1. Extract the per-cell body of `MakeJunctionTerrain` (from "auto [slope, z]" to the end of the switch) into `static void FillJunctionTerrainCell(JunctionTerrainTile &cell, TileIndex tile, uint8_t rotation, bool mirrored)` with a doc comment; `MakeJunctionTerrain` calls it; its behaviour must not change. The settings fields (foundation_cost, build_on_slopes, max_bridge_height) are set by a second static helper `static void SetJunctionTerrainSettings(JunctionTerrain &terrain)` used by both.
2. Add, declared after `MakeJunctionTerrain`:
```cpp
/**
 * Snapshot a map-axis rectangle for the line painter (LP9): cell (x, y) is map tile TileXY(x, y), orientation rotation 0, not mirrored.
 * The rectangle spans both corners (in any order) plus margin cells on every side; cells off the map are Blocked.
 * @param a One corner tile.
 * @param b The other corner tile.
 * @param margin Extra cells on every side.
 * @return The snapshot, origin at the rectangle's smallest map x and y (which may be negative).
 */
JunctionTerrain MakeJunctionTerrainRect(TileIndex a, TileIndex b, int margin);
```

## Tests (`src/tests/junction_game.cpp`, tag `[terrainrect]`), red first with a stub returning an empty terrain
1. "MakeJunctionTerrainRect: cells are map tiles": 128x128 map; raise one tile flat to 2, one corner of another by 1, a third steep (as the existing test does); rect from (40, 50) to (30, 45) (corners swapped), margin 2: origin (28, 43), width 15, height 10; At(x, y) of each modified tile gives its z_min, z_max, steep; a flat tile is Buildable at 0.
2. "MakeJunctionTerrainRect: agrees with MakeJunctionTerrain at rotation 0": for every cell of a MakeJunctionTerrain(anchor (64, 64), 0, false, default frame) snapshot that maps to a tile inside a rect over the same tiles, the two cells are equal in every field (state which OrientJunctionVector identity you rely on in a comment; if rotation 0 is not the identity map, stop and report).
3. "MakeJunctionTerrainRect: off-map cells are Blocked": rect (0, 0)-(3, 3) margin 2: origin (-2, -2), At(-1, -1) and At(-2, 3) Blocked; At(0, 0) has the same class as MakeJunctionTerrain gives for an on-map tile of the freshly allocated map (the existing test asserts that is Buildable).
4. The existing MakeJunctionTerrain test stays green unchanged.
Report the wall time of a 64x64 rect snapshot (measured in test 1 with std::chrono, printed with std::cout as `BENCH terrainrect 64x64 micros=<us>`).

## Do not
No changes to `MakeJunctionTerrain`'s signature or behaviour, the line router, the generator, the GUI, savegame, network or command code. No `std::to_string`/`std::printf` in tests; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
- Rotation 0, not mirrored, is not the identity between working cells and map tiles.
- The extraction would change `MakeJunctionTerrain`'s output.
- `[terrainrect]` over 5 s.

## Gates
0 warnings; `[terrainrect]` green under 5 s; once at the end `"[junction]~[slow]~[mapexport]"` and the `MakeJunctionTerrain` test green (report time). One commit on `wt-lp9`: `Feature: Junction painter terrain snapshot of a map rectangle`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence; functions added; suite results with times; BENCH line; stops; `git status --porcelain`, `git log -1`.

When you have reported, stop.
