# LP4: line router on flat ground (with LP3's incremental line step)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/lp4`, branch `wt-lp4`, head given in your launch prompt. You are the only writer there. Other agents work in sibling worktrees under `.claude/`; never touch them or the main worktree `/home/pete/OpenTTD`.
- Build: configure once, `cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18 -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache > cfg.log 2>&1`, then `cmake --build build-rel --target openttd_test -j 8 > build.log 2>&1` (ccache is shared, so it is fast after the first build). Debug only if the gate asks.
- Tests: `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/ptest.sh build-rel/openttd_test "<filter>" 8` (8 jobs: other agents share the machine). Iterate with your own tag; full `"~[.]"` only at the gate.
- Decisions: `/home/pete/OpenTTD/docs/junction-tool/DECISIONS.md` (read the rows named below; it is newer in the main worktree than in yours).
- Plan: `docs/junction-tool/RECON-linepainter.md` §C (Option B, the true line state) and §E LP3/LP4. Decisions D135 (c: inner track meets curve spacing, S-bends when every track meets the S-bend window, turn cost once per line), D142 (groups), D143 (narrowing: NOT this step), D134.

## Why (settled)
The painter needs an A* over whole-line moves: every move (F, L, R) advances all tracks at once, with the per-track pieces from LP2's geometry. `RasterizeJunctionLine` already computes one move's extension from the tracks' last pieces (`src/junction_line.cpp`, the loop body inside `for (size_t pos ...)`). This step exposes that as a step function and builds the router on it. Flat ground, no terrain, no bridges, obstacles as a blocked tile list.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`.
- `src/junction_line.{h,cpp}` (all), `src/tests/junction_line.cpp` (helpers `CheckLine`, `Ascii`).
- `src/junction_router.cpp` 300-345 (`TurnAllowed`, `MakePiece`: the curve grammar to replicate per track) and `src/junction_route.h` (`JunctionRouteEnd`, `JunctionPiece`, `JunctionRaster`).

## Changes
1. **Line step (LP3)**, in `junction_line.{h,cpp}`:
   ```cpp
   /**
    * Extend a line by one move: from each track's last piece, the pieces each track adds (D112, LP2 geometry).
    * @return The added pieces per track (without the last pieces), or an error text (empty vector) when the move can't be made.
    */
   std::vector<std::vector<JunctionPiece>> ExtendJunctionLine(const JunctionLineSpec &line, const std::vector<JunctionPiece> &last, char move, std::string &error);
   ```
   Move the loop body of `RasterizeJunctionLine` into it unchanged; `RasterizeJunctionLine` calls it. All existing `[junction][line]` tests must pass unchanged.
2. **Router**, new files `src/junction_line_router.{h,cpp}`, registered in `src/CMakeLists.txt`:
   ```cpp
   /** A request to route a line on flat ground (LP4). */
   struct JunctionLineRouteRequest {
   	JunctionLineSpec line;
   	TileIndexDiffC start;            ///< Track 0's first piece.
   	Trackdir start_trackdir;
   	std::vector<JunctionRouteEnd> goals; ///< Exact end pieces for track 0.
   	TileIndexDiffC min, max;         ///< Every piece of every track lies inside, inclusive.
   	std::vector<TileIndexDiffC> blocked; ///< Tiles no track may use.
   	uint curve_spacing = 0;          ///< As JunctionRouteRequest: half-units of straight run needed before a turn.
   	uint s_bend_window = 0;          ///< As JunctionRouteRequest.
   	Money rail_piece = 1;            ///< Per piece, summed over all tracks.
   	Money turn_cost = 0;             ///< Once per line turn (D135 c).
   	uint max_expansions = 200000;
   };
   struct JunctionLineRouteResult { std::optional<JunctionLineRoute> route; std::string error; };
   JunctionLineRouteResult FindJunctionLineRoute(const JunctionLineRouteRequest &request);
   ```
   - Node: track 0's last piece (tile, trackdir) plus, per track, the curve state `{u, hand, pair, prev_gap_ok}` (the router's grammar). Key: all of it.
   - Successors: F, L, R via `ExtendJunctionLine` from the tracks' last pieces (keep the last pieces of every track in the node's payload; they are a function of the path, not part of the key beyond track 0's piece and the curve states).
   - Per move, per track: replicate `Router::TurnAllowed`/`MakePiece`'s curve rule on that track's added pieces in order (a turn piece needs `TurnAllowed` on the track's state; every piece adds 1 (axis) or 2 (diagonal) to u, capped at max(curve_spacing, s_bend_window)). The move is legal only if every track passes (D135 c: the binding one is the inner track). Cite the router lines in a comment.
   - Validity, all or none: every added piece inside [min, max], not on a blocked tile, and no tile used by two tracks of the line except an exact parallel pair on one tile (Upper+Lower or Left+Right), and no tile reused by the line's own earlier pieces (no self-crossing).
   - Cost: `rail_piece` × added pieces over all tracks, + `turn_cost` once per L/R move. Heuristic: Chebyshev tile distance from track 0's tile to the nearest goal tile × `rail_piece` × tracks (admissible: each piece advances at most one tile).
   - Goal: track 0's last piece equals a goal piece (tile and trackdir).
   - Output: `JunctionLineRoute {start, start_trackdir, moves}`; `RasterizeJunctionLine` of it must reproduce the searched pieces (test). Errors: "no route" when the open set empties, "search limit" when `max_expansions` is reached.

## Tests (red first, `[junction][linerouter]`; outer first)
1. Outer: "FindJunctionLineRoute: L_R around a block": `{2,1,1}` from (10,20) X_NE... pick a start trackdir that runs along +x or −x consistently with the existing line tests, goal 20 tiles ahead on the same heading, a blocked 3×3 square straddling the straight line; route found; its raster (`RasterizeJunctionLine`) avoids every blocked tile, has no tile shared by both tracks, ends at the goal; and at least one L and one R. Red: stub returning "no route".
2. "FindJunctionLineRoute: straight when clear": same without the block → moves are all F, length equals the distance.
3. "FindJunctionLineRoute: curve spacing binds": goal reachable only with two turns closer than `curve_spacing` on a single track but routable with a longer way round inside the bounds → the found route's turns on every track are spaced per the rule (check on the raster: between two turn pieces of a track, the straight half-units ≥ curve_spacing unless the S-bend rule allows it).
4. "FindJunctionLineRoute: LL_RR keeps groups": `{4,1,2}` straight with one 45° turn → raster per track matches `RasterizeJunctionLine` of the returned moves, and the cross-line offsets after the turn are the group offsets (reuse `CheckLine` if it fits).
5. "ExtendJunctionLine reproduces RasterizeJunctionLine": for the programs of "every start and turn combination", stepping move by move gives the same pieces.
6. "FindJunctionLineRoute: no route": goal enclosed by blocked tiles → error "no route".

## Do not
- Add terrain, slopes, bridges, D143 narrowing, existing-track snapping, GUI, settings, or changes to `junction_router.cpp`.

## Stop conditions
- After a move, other tracks' last pieces are not determined by the route (the same track-0 piece and curve states reached by two paths give different track heads in a test): report it; the key would need them.
- A test needs a behaviour this brief doesn't define.

## Gates
1. Build `build-rel` `openttd_test`: no errors, no new warnings.
2. `ptest.sh build-rel/openttd_test "[junction][line],[junction][linerouter]" 8`, then `"~[.]"`.
3. Commit: `Add: Line painter router on flat ground`, body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Commit only your files.

## Report
- References read
- Commit: hash and subject
- Red evidence per test (the failure message seen before the code existed)
- Types, functions, members, constants added or removed: names only
- Suite results and wall times
- Stops, and anything left undone
- State: `git status --porcelain` and `git log -1`

When you have reported, stop.
