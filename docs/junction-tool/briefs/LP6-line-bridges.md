# LP6: bridges for the line router (D149)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/lp6`, branch `wt-lp6`, head given in your launch prompt. You are the only writer there. Never touch sibling worktrees or the main worktree `/home/pete/OpenTTD`.
- Build: configure once, `cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18 -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache > cfg.log 2>&1`, then `cmake --build build-rel --target openttd_test -j 8 > build.log 2>&1`.
- Tests: `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/ptest.sh build-rel/openttd_test "<filter>" 8`. Iterate with `"[linerouter],[line]"` or your own filter; `"[junction]~[slow]"` then `"~[.]"` at the gate.
- Hardware note: this machine's CPU is unstable under heavy all-core load. A lone segfault/SIGILL is most likely that: rerun the case alone and report it.
- Decisions: `/home/pete/OpenTTD/docs/junction-tool/DECISIONS.md` rows D135, D147, D149 (this step's ruling: read it first).

## Why (settled, D149)
The painter must cross rivers, roads and its own track; D135 d allows crossing own track and roads only by bridge. The single-track router already has `B<n>` moves (`src/junction_router.cpp` ~412-444) and `RasterizeJunctionRoute` already rasterizes `B<n>` from an axis piece. LP6 gives the line router the same move, applied to all tracks at once.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`.
- DECISIONS rows D135, D147, D149.
- `src/junction_line.h` (whole, 62 lines) and `src/junction_line.cpp` 195-270 (`ExtendJunctionLine`, `RasterizeJunctionLine`).
- `src/junction_line_router.h` (whole) and `src/junction_line_router.cpp` (whole, 288 lines).
- `src/junction_route.cpp` 56-146 (`AppendPiece`, `RasterizeJunctionRoute`, the `'B'` case).
- `src/junction_router.cpp` 257-264 (`BetweenBridgeHeads`) and 412-444 (the single-track bridge successor).
- `src/junction_terrain.h` 60-90 (`CostAt`, `SpanOk`).
- `src/tests/junction_line_router.cpp`: the helpers at the top and 232-250 (`FlatTerrainFor`, `TerrainTileOf`), and "refused over water" (283).

## Tests (red first; record each red)
Add to `src/tests/junction_line_router.cpp`, tag `[junction][linerouter]`. Use prices `rail_piece = 1`, `bridge_price = [](uint n) { return Money(10 * (n + 2)); }`, `bridge_span_ok = [](uint n) { return n <= 6; }`, `max_bridge_middle = 8`, and a river: a band of `JunctionTileClass::Water` tiles across the line's path on a `FlatTerrainFor` terrain (look up the water class's exact name in `junction_terrain.h`). Lines head along an axis (start trackdir `X_SW` or as the existing straight tests do).
1. "FindJunctionLineRoute: L_R bridges a river" (gap 1): a river 3 tiles wide that the bounds don't let the line go round. Route found; its moves contain `B3`; `RasterizeJunctionLine` of the route gives each track exactly one bridge; the two bridges' near heads differ only across the heading (equal coordinate along it), likewise the far heads; no non-bridge piece lies on a water tile. Red: before the change, "no route".
2. "FindJunctionLineRoute: LL_RR bridges a river": as 1 with LL_RR (gap 1): four bridges, heads side by side.
3. "FindJunctionLineRoute: bridges over existing track" (D135 d): no terrain; an existing layout with a straight track crossing the line's path at right angles (as "existing track is an obstacle" builds one), bounds that forbid going round. Route found with a `B` move whose middle tiles include the crossing tiles; the existing track's tiles hold no head. Red: "no route".
4. "FindJunctionLineRoute: all tracks share the bridge length": a river whose far bank is one tile further for track 1 than for track 0 (one extra water tile on track 1's row only). The route uses one `B<n>` for both tracks, with n the length track 1 needs (both far heads on land). Red: "no route" before the change.
5. "FindJunctionLineRoute: a bridge longer than the span limit is refused": test 1's river widened to 7 (`bridge_span_ok` allows up to 6): "no route". This one may pass before the change (it is a guard); record that.
6. "RasterizeJunctionLine: a bridge move": L_R gap 1, moves `"FFB2F"` on an axis heading gives each track pieces with `on_bridge` set on exactly 4 pieces (near head, 2 middle, far head) and one bridge per track; `"FLB2"` (the line is on a diagonal heading) gives an error in `rasters[0].error`; `"FB"` (no digits) gives an error. Red: errors / wrong counts before the change.
7. Extend "FindJunctionLineRoute: the route is as cheap as an uninformed search" ([slow]) or add a [slow] case beside it: tests 1-4's requests give the same `cost` with `use_heuristic` true and false. If the costs differ, stop and report (do not change the heuristic).

## Changes
1. `junction_line.h`:
   - `JunctionLineRoute::moves` doc: `///< F, L, R, and B<n> (a bridge with n middle tiles, D149).`
   - New declaration after `ExtendJunctionLine`:
     ```cpp
     /**
      * Extend a line by a bridge move B<middle> (D149): every track bridges `middle` tiles straight on from its last piece, which must be an axis piece.
      * @param line The line.
      * @param last Each track's last piece.
      * @param middle The bridge's middle-tile count.
      * @param[out] bridges One bridge per track, in track order.
      * @param[out] error The reason, when the move can't be made.
      * @return The added pieces per track (on_bridge, near head to far head), or an empty vector when the move can't be made.
      */
     std::vector<std::vector<JunctionPiece>> ExtendJunctionLineBridge(const JunctionLineSpec &line, const std::vector<JunctionPiece> &last, uint middle, std::vector<JunctionBridge> &bridges, std::string &error);
     ```
2. `junction_line.cpp`:
   - `ExtendJunctionLineBridge`: if any `last[i]` is not an axis piece (`!IsDiagonalTrackdir`, OpenTTD's name for an axis trackdir), error `" must start from axis pieces"`. Otherwise per track `RasterizeJunctionRoute({last[i].tile, last[i].trackdir, fmt::format("B{}", middle)})`; on a sub error return `fmt::format(": track {}: {}", i, sub.error)`; else take `sub.pieces` from index 1 and `sub.bridges[0]`.
   - `RasterizeJunctionLine`: on `'B'`, read the following decimal digits as in `RasterizeJunctionRoute` (error `"bridge at position {} has no length"` with none); call `ExtendJunctionLineBridge`; on failure `fail(fmt::format("move 'B' at position {}{}", pos, error))`; else append the pieces and push each bridge into `rasters[i].bridges`. Other moves unchanged.
3. `junction_line_router.h`, `JunctionLineRouteRequest`, after `turn_cost`:
   ```cpp
   std::function<bool(uint)> bridge_span_ok; ///< By middle-tile count; bridges are tried only when this and bridge_price are set and max_bridge_middle > 0 (D149).
   std::function<Money(uint)> bridge_price; ///< Per track and bridge, by middle-tile count. The heuristic stays admissible while bridge_price(n) >= (n + 2) * rail_piece.
   uint max_bridge_middle = 0; ///< Largest n tried for B<n>.
   ```
   `blocked` doc: "Tiles no track or bridge may use." Add `#include <functional>`.
4. `junction_line_router.cpp`:
   - `LineNode` gains `bool after_bridge = false; ///< The last move was a bridge (D149 5).`; `Key` appends it.
   - `LineTreeEntry::move` becomes `std::string move`; route reconstruction prepends the strings; the root's move is `""`. `added` keeps its meaning; add `std::vector<JunctionBridge> bridges; ///< The bridges this move added, one per track (D149).`
   - In `Run`, the per-expansion path state: `path_used` takes only pieces with `!on_bridge` plus the bridge head pieces (the first and last `on_bridge` piece of each bridge, i.e. every `on_bridge` piece whose tile equals a bridge's start or end); middle pieces are not occupied (D149 6). Also collect `path_bridges` (every entry's `bridges`).
   - The F/L/R loop sets `next.after_bridge = false` and is otherwise unchanged.
   - After it, when bridges are enabled (point 3) and `!node.after_bridge`: for `n = 1..max_bridge_middle` with `bridge_span_ok(n)`, call `ExtendJunctionLineBridge(request.line, node.last, n, bridges, error)` (skip on empty). The move is legal when every track's pieces pass:
     - head pieces (first and last of the track's pieces): `PieceUsable(piece)`, no entry for the tile in `existing_bits`, and no entry in `used` (the line's own tiles, including heads added by earlier tracks of this move);
     - middle pieces: `InBounds`, not in `blocked` (which already holds the existing heads), not an own head (tile equal to a `path_bridges` start or end), not between the heads of any existing or `path_bridges` bridge (a file-static `BetweenBridgeHeads(x, y, bridges)` written as `Router::BetweenBridgeHeads`, called for `request.existing->bridges` when set and for `path_bridges`), and `request.terrain->SpanOk(x, y, request.plane)` when a terrain is set;
     - `AdvanceState(next.states[i], previous, piece)` over every piece (they are axis pieces straight on, so it adds 2 units each), as the F loop does.
     Cost per track: `request.bridge_price(n) + TerrainCost(near head) + TerrainCost(far head)`; no `rail_piece`. Occupy the two heads in `used`. Push with move `fmt::format("B{}", n)`, `next.after_bridge = true`, `next.last[i]` = the far head, and the bridges.
   - `#include` what you use (`<functional>` comes through the header).

## Do not
- Change the single-track router, `RasterizeJunctionRoute`, `AddRasterToLayout`, the heuristic, the D143 narrowing (LP5b), heights (LP8) or the cursor goal (LP7).
- Allow a bridge from a diagonal-heading line (no D110 S-bend closing for lines).
- Change LP4/LP5 tests or their expectations.

## Stop conditions
- Any existing test fails (beyond a lone hardware flake).
- Test 7's informed and uninformed costs differ.
- A test needs a rule D149 doesn't give (for example a head next to an existing track's tile being refused by a check you'd have to add).

## Gates
1. Build `build-rel` `openttd_test`: no errors, no new warnings.
2. `ptest.sh build-rel/openttd_test "[junction]~[slow]" 8`, then `"~[.]"`.
3. Commit: `Feature: Junction line router bridges all tracks at once`, body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Commit only your files.

## Report
- References read
- Commit: hash and subject
- Red evidence per test
- Types, functions, members, constants added or removed: names only
- Suite results and wall times
- Stops, and anything left undone
- State: `git status --porcelain` and `git log -1`

When you have reported, stop.
