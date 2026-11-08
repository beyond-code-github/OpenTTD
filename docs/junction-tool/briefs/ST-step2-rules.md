# Brief ST step 2: station rules in the checker, with a known-good ro-ro drop fixture

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **You run in your own git worktree.** Base: `feature/junction-tool` at `{HEAD}`. Configure a Release build there (Ninja, clang-18, as in `/home/pete/OpenTTD/build-rel/CMakeCache.txt`), and build only `openttd_test`. Commit on your worktree branch; the orchestrator merges.
- **Plan:** `docs/junction-tool/PLAN-stations.md` (step ST2); decision D106; rules in `docs/junction-tool/RULES-stations.md`. Read all three.
- **v1:** a drive-through (ro-ro) drop station on a single-direction approach.
  - The in stub enters at one end; trains split to N parallel platforms, each exactly TL long; they merge after the platforms into the out stub at the far end.
  - In this step the out stub simply leaves the far side. Turning back onto the sideline's other track is ST3's concern.
- **Scratchpad:** `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/st2/`

## 2. Read first

- `src/junction_layout.h`: `JunctionLayout` (tracks, bridges and signals).
- `src/junction_route.h/.cpp`: `JunctionRoute`, `RasterizeJunctionRoute`, `JunctionNextTrackdir` and `JunctionPiece`.
- `src/junction_rules_internal.h`: helpers you may reuse, such as `TracksAt`, `IsSignalledPiece`, `IsLegalSignalPiece` and `JunctionSignalWeight`.
- `src/tests/junction_signal_pass.cpp`: the `R(...)` and `BuildRoutes` helpers, as a pattern for building fixtures from route programs.
- `CODINGSTYLE.md`.

## 3. Changes

### 3.1 Layout
`junction_layout.h` gets:

```cpp
/** A block of rail station tiles: platforms parallel to the axis (D106). */
struct JunctionStationBlock {
	TileIndexDiffC origin; ///< North corner of the block.
	Axis axis; ///< Platform direction.
	uint8_t platforms; ///< Number of platforms (the block's width across the axis).
	uint8_t length; ///< Platform length in tiles.
};
```

and `JunctionLayout` gets `std::vector<JunctionStationBlock> stations;`.
- Default empty.
- Existing code ignores it.
- `NormaliseJunctionLayout` must shift station origins like everything else; check it, and update it if it only shifts tracks, bridges and signals.

### 3.2 New files `src/junction_station_rules.h/.cpp`; add them to `src/CMakeLists.txt` like the other `junction_*` files

```cpp
/** Where trains enter and leave a station layout. */
struct JunctionStationPorts {
	JunctionRouteEnd in; ///< First piece of the in stub, heading into the layout.
	JunctionRouteEnd out; ///< Last piece of the out stub, heading out of the layout.
};

/** Rule ids for station checks (ST2). */
enum class JunctionStationRule : uint8_t { Platforms, Connectivity, Signals, Braking };

struct StationRuleViolation {
	JunctionStationRule rule;
	TileIndexDiffC tile;
	std::string message;
};

std::vector<StationRuleViolation> CheckStationRules(const JunctionLayout &layout, const JunctionStationPorts &ports, uint8_t tl, uint8_t sd);
```

**Rules:**
1. **Platforms (S13, S14, D106).**
   - Exactly one station block.
   - `length == tl`, and `platforms ≥ 2`.
   - Every block tile holds exactly the axis track, `TrackBits{AxisToTrack(axis)}`.
2. **Connectivity (S1).** Walk the track graph forward from `in`, following all branches with `JunctionNextTrackdir` over the tracks present, and not reversing.
   - Every platform row is entered by some walk.
   - From every platform's far end, `out` is reachable.
   - No walk reaches `in` again (no loops back).
   - Write a small file-static BFS over (tile, trackdir).
3. **Signals (S26, S27, D97).**
   - **(a)** A signal faces into the station on the last legal piece before the first split on the in stub.
   - **(b)** Each platform has a signal facing out on the first legal piece after the platform's far end, before any merge.
   - **(c)** There's a signal facing out after the last merge on the out stub.
   - **(d)** No signal sits on station tiles. The game forbids it, so it must not appear.
   - **(e)** Along every walked path, no gap between signals exceeds `sd · 192` drag weight where a legal piece exists. This is the junction spacing rule, so copy its loop.
4. **Braking (S19, the incoming line never slows).** From `in` to the first split there are at least `tl` tiles of track without a switch.

### 3.3 Fixture and tests (`src/tests/junction_station_rules.cpp`, registered in `src/tests/CMakeLists.txt`, tag `[junction][station]`); red first

1. **Build `MakeRoRoDropFixture(tl = 4, platforms = 2)`** with route programs and `BuildRoutes`-style rasterizing (copy the small helper; don't share across test files):
   - an in stub of ≥ 4 straight tiles;
   - a split into 2 adjacent parallel tracks (one stays straight, one shifts one row with an S-bend);
   - 2 platform rows of 4 tiles covered by a `JunctionStationBlock`;
   - the mirrored merge;
   - an out stub of ≥ 2 tiles;
   - signals placed by hand to satisfy rule 3.
   Print its ASCII in a WARN. Note that `RenderJunctionAscii` doesn't show stations; add the block's rect to the WARN text.
2. **Tests:**
   - **"a correct ro-ro drop passes":** no violations.
   - One test per rule, each a single targeted break of the fixture:
     - platform length 3;
     - one platform row disconnected (remove its merge track);
     - the entry signal removed;
     - one platform exit signal removed;
     - a signal placed on a station tile;
     - a switch moved within 2 tiles of `in` (or the in stub shortened to 2).
     Each test asserts that exactly the expected rule fires.
   - Red: write the tests first. Before `CheckStationRules` exists that's a compile/link error, which is acceptable. Then implement until they pass.

## 4. Do not

- touch the junction generator, the signal pass or the junction checker
- add station building, GUI or rig code (ST3+)

## 5. Stop conditions

- A rule from §3.2 can't be checked without game state.
- Anything not covered.

## 6. Gates

1. Release build of `openttd_test` in the worktree, then the full suite, all passing.
2. Commit: `Add: Station rule checks with a ro-ro drop fixture`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 7. Report

- the worktree branch and commit
- red evidence
- the types and functions added
- the fixture ASCII and block rect
- suite totals
- stops
- `git status --porcelain` and `git log -1`, both in the worktree

When you have reported, stop.
