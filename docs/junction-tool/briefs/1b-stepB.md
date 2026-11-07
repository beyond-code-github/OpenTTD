# Brief 1b step B: the rule checker

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`.
- **Plan:** `docs/junction-tool/PLAN-1b.md` §1, §2, §4 and Steps row B.
- **Decisions:** D24, D25, D58, D59, D67, D70, D71, D74 in `DECISIONS.md`.
- **Step A gave you:**
  - `src/junction_route.h`: `JunctionPiece`, `JunctionRoute`, `RasterizeJunctionRoute`, `AddRasterToLayout`, `NormaliseJunctionLayout`, `RenderJunctionAscii`, `TrackdirToDirection`
  - `src/junction_generator.h`: `JunctionSpec`, `MakeTestJunctionSpec`, `CurveSpeedLimitForSpacing`
  - `src/junction_rules.h`: `JunctionRule`, `RuleViolation`, and the `CheckJunctionRules` stub

## 2. Read first (once each, with offset/limit)

- `CODINGSTYLE.md`
- `src/junction_route.h` and `.cpp`; `src/junction_rules.h` and `.cpp`
- `src/junction_generator.h`
- `src/tests/junction_route.cpp` (fixture style)
- `src/track_func.h`: `TrackdirReachesTrackdirs`, `TrackdirToExitdir`, `TracksOverlap`, `TrackToTrackdir`, `ReverseTrackdir`
- `src/train_cmd.cpp`: `Train::GetCurveSpeedLimit`, which G3 reproduces literally

## 3. Design (all pure; in `junction_rules.h/.cpp`)

### 3.1 Ports

```cpp
enum class JunctionPortRole : uint8_t { EIn, EOut, WIn, WOut, NIn, SOut };
struct JunctionPort { JunctionPortRole role; TileIndexDiffC tile; Trackdir trackdir; }; ///< trackdir: travel direction on the port tile (inward for In, outward for Out).
std::vector<JunctionPort> FindJunctionPorts(const JunctionLayout &layout);
```

**Port tiles.** A port tile is a tile on the bounding box boundary (x = 0, x = size.x−1, y = 0 or y = size.y−1) holding a track that exits through the outer edge:
- NE edge at x = 0
- SW edge at x = max
- NW edge at y = 0
- SE edge at y = max

**Roles** (the canonical right-hand frame, PLAN-1b §1):
- **West edge (x = 0),** expect two port tiles with rows r1 < r2:
  - r1 is `EIn` (trackdir `X_SW`)
  - r2 is `WOut` (`X_NE`)
- **East edge (x = max):**
  - the smaller row is `EOut` (`X_SW`)
  - the larger row is `WIn` (`X_NE`)
- **Bottom edge (y = max),** columns c1 < c2:
  - c1 is `NIn` (`Y_NW`)
  - c2 is `SOut` (`Y_SE`)

**Mismatches.** Any other count, a port track that isn't X (west/east) or Y (bottom), or a port on the top edge: return what matches, and let G1 report the rest. A track exiting the top edge is never a port.

### 3.2 Check context and tracing

```cpp
struct JunctionPath { JunctionPortRole from; JunctionPortRole to; std::vector<JunctionPiece> pieces; };
struct JunctionCheckContext {
	const JunctionLayout &layout;
	const JunctionSpec &spec;
	std::vector<JunctionPort> ports;
	std::vector<std::pair<JunctionPortRole, JunctionPortRole>> expected; ///< Movements that must exist.
	std::vector<JunctionPath> paths; ///< Filled by TraceJunctionPaths.
	std::vector<RuleViolation> trace_problems; ///< Dead ends, cycles, unexpected exits.
};
JunctionCheckContext MakeJunctionCheckContext(const JunctionLayout &layout, const JunctionSpec &spec, std::vector<JunctionPort> ports, std::vector<std::pair<JunctionPortRole, JunctionPortRole>> expected);
```

`MakeJunctionCheckContext` stores its inputs and calls the tracer.

`CheckJunctionRules(layout, spec)`:
1. Builds the context from `FindJunctionPorts` and the six movements: EIn→EOut, EIn→SOut, WIn→WOut, WIn→SOut, NIn→WOut, NIn→EOut.
2. Returns the concatenation of every group in the order G1…G10. G3 is skipped when `!spec.curve_limit`.

Each group is a public function taking `const JunctionCheckContext &` and returning `std::vector<RuleViolation>`, so that tests can call one group on a small fixture: `CheckConnectivity`, `CheckGeometry`, `CheckCurveSpeed`, `CheckOrder`, `CheckBridges`, `CheckSignals`, `CheckWaitingRoom`, `CheckDeadlock`, `CheckPrio`, `CheckFootprint`.

**Tracer** (inside `MakeJunctionCheckContext`). For each `In` port, run a depth-first search from the piece `(port.tile, port.trackdir)`. Next piece from `(t, td)`:
1. `n = t + TileIndexDiffCByDiagDir(TrackdirToExitdir(td))`.
2. **Off the box:** if `n` is outside the box, the path ends. It's complete if `(t, td)` is an `Out` port's tile and trackdir; otherwise it's a trace problem (Connectivity, "unexpected exit").
3. **Bridge head:** if `n` is a bridge head of a layout bridge whose axis matches `td`'s axis, and the travel enters it from outside the bridge:
   - append pieces with `on_bridge = true`, 2 units each, for the head, every span tile and the far head
   - continue from the far head with the same `td`
4. **Otherwise:** the candidates are `TrackdirReachesTrackdirs(td)` ∩ the trackdirs present on `n`. A track's trackdirs are `TrackToTrackdir(track)` and its reverse.
   - No candidates: a trace problem (dead end).
   - Several candidates: branch.
5. **Cycles:** revisiting a `(tile, trackdir)` already on the current path is a trace problem (cycle). Cap paths at 4096 pieces (problem: "path too long").
6. **Units:** an axis piece is 2, a half piece 1, as in step A.

### 3.3 Shared helpers (internal, or in the header if a test needs them)

- **Switch events along a path** (index k into `pieces`):
  - **Split at piece k+1:** more than one candidate (as in tracer step 4) from piece k. The switch tile is `pieces[k+1].tile`.
  - **Merge at piece k+1:** another trackdir `t' ≠ pieces[k].trackdir` on `pieces[k].tile`, with `TrackdirToExitdir(t') == TrackdirToExitdir(pieces[k].trackdir)` and `pieces[k+1].trackdir ∈ TrackdirReachesTrackdirs(t')`. The switch tile is `pieces[k].tile`.
  - Bridge pieces are never switches.
- **Legal signal piece:**
  - the piece isn't on a bridge
  - the tile's bits are not `TracksOverlap`
  - the tile isn't a bridge head
  - the piece's track has no signal yet, on either trackdir
- **Signal on a piece:** a `JunctionSignal` with that tile and `facing == trackdir`.
- **Blocks** (`std::map`/`flat_map` from tile to block id; signal tiles get no block):
  - Flood over tiles. Two tiles A and B, adjacent across edge e, are joined when a track on A touches e, a track on B touches the opposite edge, and neither A nor B holds any signal.
  - A track touches the edges `TrackdirToExitdir` of its two trackdirs.
  - **All track bits of a tile are one unit.** This models `signal.cpp` flooding a whole tile, including X+Y crossings (RECON-path-prio §2).
  - **A bridge joins** the tile before its near head and the tile after its far head (both heads belong to that block).

### 3.4 The groups (rule enum value in brackets)

**G1 `CheckConnectivity` [Connectivity]:**
- Emit the trace problems.
- For each expected movement, exactly one path. Report zero ("missing movement") and more than one ("ambiguous movement").
- Any path whose (from, to) isn't expected is a violation.
- Any expected role with no port is a violation ("missing port").

**G2 `CheckGeometry` [Geometry]:** a tile where both X and Y are used by paths (a level crossing). A track is used if some path has a piece on that tile with that track.

**G3 `CheckCurveSpeed` [CurveSpeed].** For each path:
1. Expand it into a unit-direction list: each piece contributes `units` copies of `TrackdirToDirection(td)`.
2. Pad with `2×TL` copies of the first direction in front and `2×TL` copies of the last direction behind (straight legs).
3. For every window of `n = 2×TL` consecutive entries, set `front = window[n−1]` and walk parts from front to back exactly as `GetCurveSpeedLimit` walks `u` and `u->Next()`, with `cached_veh_length = 1` and `VEHICLE_LENGTH = 1`:
   - `dirdiff = DirDifference(dir[u], dir[u.next])`
   - the `curvecount` Left45/Right45 counting
   - `numcurve`, `sum`, `pos`, `lastpos`
   - `pos - lastpos <= 1` gives 88
   - 90° gives 61
   - the S-bend exemption when exactly one Left45 and one Right45
   - otherwise `CeilDiv(sum,1)/numcurve` into `232 − (13 − Clamp(sum, 1, 12))²`
   - then, only if the limit isn't "unlimited", the curve bonus `max += (max / 2) * curve_speed`
4. A limit below `spec.reference_speed` is a violation at the tile of the piece under the front.
5. Report each path's first failure only.

**G4 `CheckOrder` [Order]:** along each path, a merge event followed later by a split event is a violation.

**G5 `CheckBridges` [Bridges]:** for each bridge:
- heads on one row or column, and distinct
- no track bits and no signal on either head
- the tile outside each head, along the axis, has that axis track
- `spec.bridge_span_ok(middle)` with `middle = distance − 1`
- no tile (head or span) shared with another bridge

**G6 `CheckSignals` [Signals]:**
1. **Legality:** every signal's tile has its track, isn't overlap, and isn't a head.
2. **Facing:** a signal on a track used by paths must face along a used trackdir. A signal on an unused track is left to G9.
3. **Switch isolation:**
   - For each split (switch tile T on a path), the nearest legal-or-signalled piece before T on that path, stopping at the previous switch, a bridge piece or the path start, carries a signal facing along the path.
   - Likewise before each merge, on each path that enters T.
   - After each merge, the nearest such piece after T, stopping at the next switch, a bridge or the path end.
   - If the stretch has no candidate piece, that's no violation.
4. **Spacing:** walk each path with `d` = the sum of units of the pieces after the last signal piece, up to and including the current one (d starts at 0 at the path start). A piece that is legal, unsignalled and has `d > 2 × spec.sd` is a violation. Report it once, then reset d.
5. **Types:** every route signal has `type == spec.signal_type`, except the joiner's before-merge signal at an ML merge, which must be `Entry` (checked in G9; skipped here).

**G7 `CheckWaitingRoom` [WaitingRoom]:**
- For each path EIn→SOut and WIn→SOut, take the first split S on the path (the switch where it leaves the ML) and the first merge M after S.
- The units of the pieces strictly after S's switch piece, up to and including the last signal piece before M's switch tile, must be ≥ `2 × spec.tl`.
- Missing S, M or the signal is a violation.

**G8 `CheckDeadlock` [Deadlock]:** compute blocks. A block containing a split switch tile and a merge switch tile (from any paths) is a violation.

**G9 `CheckPrio` [Prio]:**
- **ML merges:** merges on the EIn→EOut and WIn→WOut paths. The joiner is the other path entering that switch tile.
- **Prio components:** connected sets of unused track bits. Two unused bits are connected when they're on adjacent tiles across an edge both touch. Bits on the same tile are not connected to each other.

For each ML merge M on ML path p, there must be exactly one component C whose bits share a block with M's switch tile. Then check:
1. **Shape.**
   - Every non-rung tile of C (one not on p) is in the single row or column adjacent to p's track at M, holding only p's axis track, or that axis plus the perpendicular (a rung).
   - A rung is a tile of p whose bits are exactly X|Y, with the perpendicular unused, adjacent to a C tile with X|Y.
2. **C touches nothing used.** For every bit of C and every edge it touches, any track on the neighbour tile touching the opposite edge is in C.
3. **Downstream rung.**
   - C has a rung whose tile is after M's switch on p and lies in M's block.
   - p's first signal after M comes after that rung (D59, R26).
4. **Coverage.**
   - Walk p backwards from M's switch tile, summing units, until the sum ≥ `2 × spec.tl`.
   - Every block met (except M's) must contain exactly one rung of C.
   - Each block that C's row tiles fall in contains exactly one rung (one segment per rung).
5. **Prio signals.**
   - The signals on C's row tiles face along C's axis, away from M (upstream).
   - There is one between every pair of consecutive segments.
   - The farthest from M is `Exit`; every other one is `Combo`.
6. **Joiner signal:** the joiner's nearest signal before M is `Entry`.

Also:
- Any component not matched to an ML merge is a violation ("stray track").
- An ML merge with no component is a violation ("missing prio").

**G10 `CheckFootprint` [Footprint]:**
- The minimum offsets are 0, and `size` equals max + 1 over tracks and bridge ends.
- Boundary tiles on the west, east and bottom edges hold no signal, no switch tile, no bridge head and no unused bit. The top edge is exempt.

## 4. Tests: `src/tests/junction_rules.cpp`

Register the file in `src/tests/CMakeLists.txt`.

**For each group:**
- one passing fixture
- at least one failing fixture that breaks only that rule, asserting the violation's rule and tile
- every clause of G6 and G9 gets its own failing fixture

**Build fixtures** with step A's route programs plus hand-added signals and bridges, and pass explicit ports and expected movements to `MakeJunctionCheckContext`. Small fixtures are fine: e.g. a single straight ML with one prio for G9, or a two-port line for G3. Put a `RenderJunctionAscii` dump in a comment above each non-trivial fixture.

**G3 reference cases** (worked by hand from the game formula; TL = 4, so n = 8 parts):
- a straight path never fails
Half-piece parity makes same-direction bends through a diagonal an odd number of units apart.
- From `X_SW`, `"LL"` (bends 1 unit apart) has limit 88: fails at 89, passes at 88.
- `"LFFL"` (3 units apart) has limit 132: fails at 140, passes at 132.
- `"LFR"` (one S-bend, 2 units apart) alone in the train passes at any speed (exempt).
- `"LFRLFR"` (two S-bends: curvecount 2 and 2, numcurve 3, average spacing 2) has limit 111: fails at 200 and at 112, passes at 111.

**Outer test:** step A's `[!mayfail]` test must still compile. Leave it tagged.

Red first: write each group's failing test, see it fail on an assertion (with the group stubbed to return `{}`), and record the message.

## 5. Do not

- Don't generate layouts or design templates (steps C/D).
- Don't touch non-junction upstream files except `src/tests/CMakeLists.txt`.
- Don't change step A's public signatures.
- Don't remove `[!mayfail]`.

## 6. Stop conditions

- A clause above is ambiguous for a fixture you need.
- `GetCurveSpeedLimit` differs from §3.4 G3.
- A G3 reference case's hand-worked expectation contradicts your literal implementation. Report both numbers; don't adjust either.
- Anything not covered.

## 7. Gates

1. `cmake --build build --target openttd openttd_test > /tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/stepB_build.log 2>&1`, then grep for `error|warning`. There must be none in the junction files.
2. Full suite to `stepB_test.log`. Expect everything to pass except the `[!mayfail]` outer test.

Commits (signing is off; end each message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`):
- `Feature: Junction rule checker - ports, tracing, connectivity, geometry, order, bridges`
- `Feature: Junction rule checker - curve speed, signals, waiting room, deadlock, prio, footprint`

## 8. Report

- references read
- commits
- red evidence per group
- types and functions added
- suite totals (baseline 143 cases)
- stops or anything undone
- `git status --porcelain` and `git log -1`

When you have reported, stop.
