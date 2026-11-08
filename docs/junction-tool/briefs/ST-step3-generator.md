# Brief ST step 3: GenerateStation, a pure generator for a ro-ro drop station (v1)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **You run in your own git worktree.** Base: `feature/junction-tool` at `{HEAD}`. Configure a Release build there (Ninja, clang-18, like `/home/pete/OpenTTD/build-rel`), and build only `openttd_test`. Commit on the worktree branch; the orchestrator merges.
- **Read:** `docs/junction-tool/PLAN-stations.md` (ST3), D106, `RULES-stations.md` and D97 (drag signal spacing).
- **ST2 is merged.**
  - `JunctionStationBlock` and `JunctionLayout::stations` are in `src/junction_layout.h`.
  - `src/junction_station_rules.h/.cpp` has `JunctionStationPorts` and `CheckStationRules(layout, ports, tl, sd)`.
  - `src/tests/junction_station_rules.cpp` has `MakeRoRoDropFixture`, a hand-built known-good 2-platform station. Mirror its geometry.
- **Scratchpad:** `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/st3/`

## 2. Read first

- `src/tests/junction_station_rules.cpp`, all of it: the fixture geometry and the signal positions.
- `src/junction_station_rules.h/.cpp`: what the rules check.
- `src/junction_router.h`: `JunctionRouteRequest`, `FindJunctionRoute`, and the split-start rule (Exception 1) that lets a route start from a piece already laid.
- `src/junction_route.h/.cpp`: `RasterizeJunctionRoute`, `AddRasterToLayout`, `NormaliseJunctionLayout` and `JunctionSignalWeight` (in `junction_rules_internal.h`).
- `src/junction_generator.h`: `JunctionSpec` (tl, sd, curve fields, signal_type, prices, bridge_span_ok) and `DeriveCurveSpacing`.

## 3. Changes

### 3.1 New files `src/junction_station_generator.h/.cpp`; add them to `src/CMakeLists.txt`

```cpp
/** Result of generating a station (D106 v1: ro-ro drop, one incoming direction). */
struct JunctionStationResult {
	std::optional<JunctionLayout> layout; ///< Normalised; trains travel toward +x.
	JunctionStationPorts ports; ///< In and out, in the normalised layout's coordinates.
};

/** Generate a ro-ro drop station with the given number of platforms (2..8), each exactly spec.tl long. */
JunctionStationResult GenerateStation(const JunctionSpec &spec, uint8_t platforms);
```

### 3.2 Geometry (canonical frame; trains travel +x, `Trackdir::X_SW`)

- **The platform block:** rows `y = 0 .. platforms−1`, and x from `P0` to `P0 + tl − 1`, with `P0` from the sweep below. Lay each platform row as straight track and add the `JunctionStationBlock`.
- **In stub:** row 0, from x = 0, straight, `B` tiles long with `B ≥ tl` (rule S19, braking) before the first split.
- **Entry fan:**
  - Row 0 continues straight into platform 0.
  - For platforms k = 1..N−1, in order, route with `FindJunctionRoute`:
    - starts: any plain axis piece already laid on the in line or earlier branches, west of `P0` and at x ≥ B (the split-start rule);
    - goal: the piece entering platform k's west end, heading `X_SW`;
    - bounds: x 0 .. `P0`, y 0 .. N−1;
    - curve fields from `spec`, as `BuildJunctionCandidate`'s `base` request sets them;
    - no bridges: `max_bridge_middle = 0`, or `bridge_span_ok` always false.
  - Add each route's raster.
- **Exit fan:** mirror it. Row 0 continues straight from platform 0's east end to the out stub. For k = 1..N−1, route from the piece leaving platform k's east end to any plain axis piece on row 0, east of the block. The out stub on row 0 runs at least 2 tiles beyond the last merge.
- **The sweep:** `P0` from `B + 2` upward (up to `B + 4·N`, or until a candidate builds). Keep the first candidate that is fully clean under `CheckStationRules`. If none is clean, return no layout.

### 3.3 Signals (all `spec.signal_type`, facing `X_SW`, along the direction of travel), to satisfy `CheckStationRules` rule 3

1. **Entry:** on the last legal piece before the first split on the in line.
2. **Platform exits:** on each branch, the first legal piece after the platform's east end, before any merge.
3. **After the last merge:** on the out stub, on the first legal piece after the merge.
4. **Fill** along each walked path (the in stub up to the entry signal, and the out stub after its signal) using the drag rule from D97: weights from `JunctionSignalWeight`, density `spec.sd · 192`, minimise gaps, walking from the downstream anchor. Write it as a small file-static function; don't reuse the junction signal pass.

### 3.4 Ports
- `in` is the first piece of the in stub (x = 0, row 0, `X_SW`).
- `out` is the last piece of the out stub.
- Normalise the layout, then shift the ports by the same offset.

## 4. Tests (`src/tests/junction_station_generator.cpp`, tags `[junction][station]`); red first

1. For `MakeTestJunctionSpec()` (tl 4, sd 2) and N = 2, 3 and 4: `GenerateStation` gives a layout, and `CheckStationRules` is empty. WARN the ASCII and the block rect.
   - Red: before `GenerateStation` exists, this is a link or compile error (acceptable; say so).
2. **Deterministic:** the same spec gives the same layout (compare with `JunctionLayoutToJson`).
3. **Each platform is exactly tl long:** the block's length is tl, and its platform count is N.
4. Report each N's size (w×h).

## 5. Do not

- touch the junction generator, the signal pass or the junction checker
- add game-side building, GUI or rig code (ST4 and ST5)
- support terminus, pickup or multiple incoming lines

## 6. Stop conditions

- N = 2 can't be generated clean. Report the least-bad candidate's violations and its ASCII.
- The router's split-start rule doesn't allow fans. Report what fails.
- Anything not covered.

## 7. Gates

1. Release build of `openttd_test` in the worktree, then the full suite, all passing.
2. Commit: `Add: Station generator for ro-ro drop stations`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 8. Report

- the branch and commit
- red evidence
- the sizes and ASCII for N = 2, 3 and 4
- suite totals
- stops
- `git status --porcelain` and `git log -1`, both in the worktree

When you have reported, stop.
