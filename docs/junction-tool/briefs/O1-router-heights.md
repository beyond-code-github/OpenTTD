# O1: heights in the line router (PLAN-organic O1, D152)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/o1`, branch `wt-o1`, head given in your launch prompt. You are the only writer there. Never touch sibling worktrees or the main worktree `/home/pete/OpenTTD`.
- Build: configure once, `cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18 -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache > cfg.log 2>&1`, then `cmake --build build-rel --target openttd_test -j 8 > build.log 2>&1`.
- Tests: `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/ptest.sh build-rel/openttd_test "<filter>" 8`. Iterate with `"[linerouter]~[slow]"` or your own filter; `"[junction]~[slow]"` then `"~[.]"` at the gate.
- Hardware note: this machine's CPU is unstable under heavy all-core load. A lone segfault/SIGILL is most likely that: rerun the case alone and report it.
- Plan: `/home/pete/OpenTTD/docs/junction-tool/PLAN-organic.md`. Decisions: `DECISIONS.md` rows D95, D147, D149, D150, D152 (this step's ruling: read it first).

## Why (settled, D152)
Junctions and painted lines must follow the land: track climbs and descends on inclined slopes instead of the whole layout sitting on one plane. The line router (LP4-LP7) today fixes one `plane`. O1 gives every track its own height in the search state and adds ramps. Tunnels (O2) and mounds (O3) come later.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`.
- DECISIONS rows D95, D152.
- `src/junction_terrain.h` (whole, ~92 lines).
- `src/junction_game.cpp`: `MakeJunctionTerrain` (grep it; ~40 lines).
- `src/junction_layout.h` 115-130 (`OrientJunctionVector`, `InverseOrientJunctionVector`).
- `src/slope_func.h` 230-260 (`IsInclinedSlope`, `GetInclinedSlopeDirection`).
- `src/junction_line_router.h` (whole) and `src/junction_line_router.cpp` (whole).
- `src/tests/junction_line_router.cpp`: helpers at the top, `FlatTerrainFor`, `TerrainTileOf`, "refused over water", one LP6 bridge test, the `[slow]` informed = uninformed cases.

## Tests (red first; record each red)
Router tests in `src/tests/junction_line_router.cpp`, tag `[junction][linerouter]`; L_R gap 1 on an axis heading; terrain from `FlatTerrainFor` with heights and `incline` set by hand; bounds that forbid going round.
1. "FindJunctionLineRoute: climbs over a ridge": a ridge across the path: one row of tiles inclined up (along the heading, z_min 0, z_max 1), two rows flat at 1, one row inclined down (rising towards the start), then flat 0 again; goal beyond at height 0. Route found; its cost equals the hand sum (pieces, ramps with `slope_piece`, terrain). Red: no route (one plane).
2. "FindJunctionLineRoute: descends into a valley": the mirror image (down, flat at -1 ... use heights 1 and 0 with the start at 1 if negative heights are awkward), route found.
3. "FindJunctionLineRoute: no turn on a ramp": a ramp tile placed where the cheapest flat-ground route would turn; the found route has no turn piece on any inclined tile (check every piece of every track against the terrain).
4. "FindJunctionLineRoute: a bridge or slopes by price": a valley one level deep and 3 tiles wide with ramps on both sides; with bridges enabled (LP6 setup), `slope_piece` 0 gives a route without a `B` move, and `slope_piece` 1000 gives a route with a `B` move. Red: no route with only one plane.
5. "JunctionCanonicalIncline: rotations": the new pure helper (Changes 1) for each inclined `Slope` and each rotation 0..3, mirrored false and true, gives the direction that `InverseOrientJunctionVector` gives for the slope's uphill vector; a non-inclined slope gives `DiagDirection::Invalid`. Red: link error or wrong values before the helper exists (record the first failing value after a stub returning Invalid).
6. A `[slow]` case: tests 1-4's requests give equal `cost` with `use_heuristic` true and false. If they differ, stop and report.

## Changes
1. `junction_terrain.h`:
   - `JunctionTerrainTile` gains `DiagDirection incline = DiagDirection::Invalid; ///< The canonical direction an inclined slope rises in, Invalid otherwise (D152).`
   - New free function declared here, defined in `junction_game.cpp` beside `MakeJunctionTerrain` (or in a .cpp the header already pairs with, if any; say which):
     ```cpp
     /** The canonical uphill direction of an inclined slope under a junction orientation, or DiagDirection::Invalid when the slope isn't inclined (D152). */
     DiagDirection JunctionCanonicalIncline(Slope slope, uint8_t rotation, bool mirrored);
     ```
     It takes `GetInclinedSlopeDirection(slope)`, turns it into a vector with `TileIndexDiffCByDiagDir`, maps it with `InverseOrientJunctionVector`, and returns the `DiagDirection` of the result.
   - `MakeJunctionTerrain` sets `cell.incline = JunctionCanonicalIncline(slope, rotation, mirrored)`.
2. `junction_line_router.h`, `JunctionLineRouteRequest`:
   - `plane` doc: "Track 0's start height; every track starts at it (D152)."
   - New after `turn_cost`: `Money slope_piece = 0; ///< Added for each ramp piece, per track (D95, D152).`
3. `junction_line_router.cpp`:
   - `LineNode` gains `std::vector<int8_t> z; ///< Each track's height after its last piece (D152).` (root: all `request.plane`); `Key` appends every z.
   - A per-piece height step, used for F/L/R pieces of every track in order: given the track's current z and the piece:
     - level: `request.terrain == nullptr` or `UsableAt(x, y, z)` — the piece stays at z (today's rule);
     - else, only for an axis piece (`IsDiagonalTrackdir`, OpenTTD's name for axis) on a Buildable, non-steep tile with `incline != Invalid`: up when `incline == TrackdirToExitdir(piece.trackdir)` and `z_min == z` (z becomes z + 1); down when `incline == ReverseDiagDir(TrackdirToExitdir(piece.trackdir))` and `z_max == z` (z becomes z - 1);
     - else the piece is not usable.
     `PieceUsable` keeps its bounds, blocked and existing checks and takes the height step's verdict in place of its `UsableAt(... request.plane)` call. A turn move (L/R) may not use a ramp for any piece (level only).
   - Cost: a level piece pays `TerrainCost` at its z as today; a ramp piece pays its tile's `clear_cost` (no foundation) plus `request.slope_piece`, still once per tile for the terrain part.
   - Bridges (D149): every check that used `request.plane` uses the track's z: heads usable level at z, span `SpanOk(x, y, z)`; a track's z is unchanged by a bridge.
   - Start pieces: checked level at `request.plane` as today.
4. Without a terrain everything behaves exactly as today (all tracks stay at `plane`).

## Do not
- Change the single-track router (`junction_router.cpp`), the generator, `RasterizeJunctionLine`, `JunctionPiece`, tunnels (O2) or terraform (O3).
- Allow ramps on diagonal pieces or in turn moves, or steep slopes.
- Change existing tests' expectations.

## Stop conditions
- Any existing test fails (beyond a lone hardware flake).
- Test 6's costs differ.
- `MakeJunctionTerrain`'s orientation can't be inverted with `InverseOrientJunctionVector` as described (report what you found).

## Gates
1. Build `build-rel` `openttd_test`: no errors, no new warnings.
2. `ptest.sh build-rel/openttd_test "[junction]~[slow]" 8`, then `"~[.]"`.
3. Commit: `Feature: Junction line router climbs and descends inclined slopes`, body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Commit only your files.

## Report
- References read
- Commit: hash and subject
- Red evidence per test
- Types, functions, members, constants added or removed: names only
- Suite results and wall times
- Stops, and anything left undone
- State: `git status --porcelain` and `git log -1`

When you have reported, stop.
