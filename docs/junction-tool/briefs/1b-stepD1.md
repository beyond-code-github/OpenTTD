# Brief 1b step D1: route search core (`FindJunctionRoute`)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Worktree:** `{WORKTREE}`, branch `{BRANCH}`, head `{HEAD}`.
  - Build dir: `{BUILD}`. Configure it once with `cmake -S . -B {BUILD} -DCMAKE_BUILD_TYPE=Debug > {LOG}/cfg.log 2>&1` if it does not exist.
  - Another agent is working in `/home/pete/OpenTTD`. Do not touch that directory.
- **Plan:** `docs/junction-tool/GENERATOR-1b.md` (D87), step D1. Read it.

## 2. Read first (once, with offset/limit)

- `CODINGSTYLE.md`: the naming, brace and doc-comment sections only
- `src/junction_route.h` (all) and `src/junction_route.cpp` lines 40–140: the move semantics. You reuse them; you must not re-derive them.
- `src/junction_generator.h`: `JunctionUnitPrices`, `JunctionSpec`, `DeriveCurveSpacing`
- `src/junction_layout.h`: `JunctionLayout`, `JunctionTrack`, `JunctionBridge`
- `src/tests/junction_route.cpp`, to copy its test style and includes
- `src/tests/junction_rules.cpp` lines 391–420 (`Line`, `LinePorts`, `CurveViolations`): the pattern for checking a moves string against G3

## 3. Changes

### 3.1 Expose the stepping function
In `src/junction_route.cpp`, rename the static `FindNextTrackdir` to `JunctionNextTrackdir`, make it non-static, and declare it in `junction_route.h` with its existing doc comment moved there. Update its one caller. Nothing else in that file changes.

### 3.2 New files `src/junction_router.h` / `src/junction_router.cpp`
Add both files to `src/CMakeLists.txt` next to the other `junction_*` entries, in alphabetical order. Use the file header and include guard style of `junction_route.h`.

```cpp
/** One piece a route may start from or end on: an existing piece of the layout. */
struct JunctionRouteEnd {
	TileIndexDiffC tile;
	Trackdir trackdir;
};

/** What to route. */
struct JunctionRouteRequest {
	std::vector<JunctionRouteEnd> starts; ///< The route leaves from one of these (its first move continues from it).
	std::vector<JunctionRouteEnd> goals; ///< The route's last piece is one of these.
	TileIndexDiffC min; ///< Inclusive bounds for every piece and bridge tile the route makes.
	TileIndexDiffC max;
	uint curve_spacing; ///< CL in units (DeriveCurveSpacing).
	uint s_bend_window; ///< 2 * TL in units.
	std::function<bool(uint)> bridge_span_ok; ///< By middle-tile count, as JunctionSpec.
	uint max_bridge_middle; ///< Largest n tried for B<n>.
	JunctionUnitPrices prices;
	std::vector<TileIndexDiffC> crossable; ///< Tiles whose tracks will be lifted onto a bridge: treated as empty ground.
	Money crossable_cost; ///< Added for each piece made on a crossable tile.
	uint max_expansions = 200000;
};

std::optional<JunctionRoute> FindJunctionRoute(const JunctionLayout &layout, const JunctionRouteRequest &request);
```

The result is a `JunctionRoute` whose `start`/`start_trackdir` is the chosen start and whose `moves` uses only `F`, `L`, `R` and `B<n>`. `RasterizeJunctionRoute` of it must succeed, and its last piece must equal a goal.

#### Search
An A* over states. The cost so far is g; f = g + h.

- **Moves from a state, tried in this order:** `F`, `L`, `R`, then `B1` .. `B<max_bridge_middle>`.
  - **F, L and R:** use `JunctionNextTrackdir` with `DirDiff::Same`, `Left45` or `Right45`. The new piece's tile is the current tile stepped by `TrackdirToExitdir(current)`, exactly as `AppendPiece` does it. If `JunctionNextTrackdir` returns false, the move is illegal.
  - **B<n>:** only from an axis trackdir (`IsDiagonalTrackdir`). It makes n+2 tiles in the current direction: the near head, n middle tiles, then the far head. The trackdir stays the same. Use `bridge_span_ok(n)`.
- **Piece rule for F/L/R** (tile T, track t = `TrackdirToTrack`):
  - T is within bounds.
  - T is not a head of any layout bridge.
  - For every track e already on T in `layout.tracks`, unless T is in `crossable`: {e, t} is {Upper, Lower} or {Left, Right}.
  - **Exception 1:** the first move from a start may put its piece on a tile that holds the start's track. That tile is the split.
  - **Exception 2:** a piece may share a tile with a goal line's track (the merge tile) only when the very next F/L/R move from it makes a goal piece. Expand that pair as one successor, costing two pieces.
  - A goal piece itself lies on existing track. It is reached only through Exception 2 or by a move whose piece equals a goal.
- **Bridge rule:**
  - Both heads are within bounds, hold no track in `layout.tracks`, are not crossable, and are not layout bridge heads.
  - No middle tile is a layout bridge head, and none lies strictly between the heads of any layout bridge.
  - Middle tiles may hold track: that is the crossing.
- **Curve grammar.** The state carries:
  - u = units since the last turn, capped at max(curve_spacing, s_bend_window);
  - the last turn's hand;
  - `prev_gap_ok`: whether the turn before the last was at least s_bend_window units before the last turn, or there was none;
  - `pair`: whether the last turn closed a tight S-bend.

  Counting units:
  - every F/L/R piece adds its units (2 for axis, 1 for half pieces, as in `AppendPiece`);
  - a bridge adds 2 per tile;
  - L and R are turns; F and B are not.

  A new turn of hand h is allowed when:
  - if `pair` is set: u ≥ s_bend_window, and the new turn then clears `pair`;
  - otherwise, if u ≥ curve_spacing: allowed;
  - otherwise, if h differs from the last hand and `prev_gap_ok` holds: allowed, and the new turn sets `pair`;
  - otherwise: not allowed.

  At a start, treat the state as having no previous turns: u is at its cap and `prev_gap_ok` is true.
- **Costs:**
  - `prices.rail_piece` per F/L/R piece;
  - `prices.bridge(n)` per bridge (its head and middle pieces cost nothing more);
  - `crossable_cost` per piece made on a crossable tile.
- **Heuristic:** h = `prices.rail_piece` × min over goals of (|dx| + |dy|) between the state's tile and the goal tile.
- **State key:** (tile, trackdir, u, last hand, `prev_gap_ok`, `pair`). Keep the best g per key.
- **Determinism:** order the open set by (f, g, insertion counter). Expand starts in the order given.
- **Stop:** pop a goal piece and return its path. If the expansions exceed `max_expansions`, or the open set empties, return `std::nullopt`.

Self-crossing is not checked during the search. Before returning, rasterize the result. If two of its pieces share a tile with tracks that are not a parallel pair, return `std::nullopt` (this should not happen for a cheapest route).

### 3.3 Tests: new `src/tests/junction_router.cpp`, registered in `src/tests/CMakeLists.txt`
Tag `[junction][router]`. Write each test first, see it fail for the stated reason (an assertion, not a link error: add a stub `FindJunctionRoute` that returns `std::nullopt` first), and record the red message.

1. **Straight:** from (0,0) `X_SW` to the goal (6,0) `X_SW` on an empty layout gives `"FFFFFF"`.
2. **Turn:** from (0,0) `X_SW` to the goal (6,6) `Y_SE` in bounds (0,0)–(12,12), with the test spec's CL and TL (`MakeTestJunctionSpec`, `DeriveCurveSpacing`):
   - the result rasterizes, and its last piece is the goal;
   - the route has no G3 violation, checked with the `CurveViolations` pattern (copy the helper into this file);
   - no two same-hand turns are closer than CL units (scan the raster).
3. **Obstacle:**
   - a wall of `Track::Y` tracks at x=5, y=0..12, with bounds that leave no way round, forces a `B` move over it;
   - with the bounds widened so a detour exists and bridges priced very high, the route avoids `B`.
4. **Crossable:** the same wall listed as `crossable`, with crossable_cost 0, gives a route with no `B` that passes through x=5 at ground level.
5. **Split start:** the start (3,0) `X_SW` on a layout holding row 0 as `X` from 0..10:
   - the first piece leaves row 0 by `L` or `R`;
   - the first produced piece's tile holds the row's X track. Exception 1 is what allows this.
6. **Merge goal:** goals = every piece of a laid row 0 line (`X_SW`, x 0..20), start (2,8) `Y_NW`:
   - the route ends on a row-0 piece;
   - its second-to-last piece shares a tile with the row's X track. Exception 2 is what allows this.
7. **Unreachable:** a goal enclosed by tracks, with no bridge allowed (`bridge_span_ok` false), gives `std::nullopt`.
8. **Deterministic:** the same request twice gives identical moves.
9. **S-bend grammar:** from (0,0) `X_SW` to the goal (12,1) `X_SW`, the result contains one tight S-bend (an opposite turn within CL units), and the next turn after it is at least s_bend_window units later, or there is none.

## 4. Do not

- touch any file not named here
- change `RasterizeJunctionRoute`'s behaviour
- add search features not listed (no jump points, no cost caching, no multi-route search)
- use `std::unordered_map` keyed on structs without a deterministic hash. Use `std::map`, or a hash over the integer fields

## 5. Stop conditions

- A test's stated expectation turns out false for a reason in the move semantics. For example, test 9 has no tight S-bend at all because a straight-plus-turns route is cheaper. Report the route you got; do not tune the costs.
- Anything not covered.

## 6. Gates

1. `cmake --build {BUILD} --target openttd openttd_test > {LOG}/d1_build.log 2>&1`. There must be no errors or warnings in junction files.
2. `{BUILD}/openttd_test > {LOG}/d1_test.log 2>&1`. Everything passes except the `[!mayfail]` outer generator test. The baseline is 182 cases plus yours.

Commit message: `Add: Junction route search for the generator`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 7. Report

- references read
- commit
- red evidence per test
- types and functions added
- suite totals
- for tests 2, 3, 6 and 9, the moves string returned
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
