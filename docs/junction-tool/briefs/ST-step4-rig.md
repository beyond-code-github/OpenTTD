# Brief ST step 4: stations on the rig (JSON export, GS build, debug generate-at-tile)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **You run in your own git worktree,** based on `worktree-agent-a0fe15fc312fa9b81` at `49cc448573`. That is ST3, the station generator, which isn't merged into `feature/junction-tool` yet.
  - First run `git checkout -B st4 49cc448573` in your worktree.
  - Configure **both** a Release build (`openttd_test`) and a **Debug** build (`openttd`, for the rig server), with Ninja and clang-18, in build dirs inside the worktree.
  - Commit on your branch.
- **Read:**
  - `docs/junction-tool/PLAN-stations.md` (ST4);
  - D106;
  - `tools/ottd/README.md`;
  - `src/junction_station_generator.h`;
  - `src/junction_layout.h/.cpp` (`JunctionLayoutToJson`);
  - `src/console_cmds.cpp` (`junction_export`, debug-only: the pattern to copy);
  - `tools/ottd/ottd` (`layout_batches`, `cmd_build_layout`, `junction_here`);
  - `tools/ottd/gs/main.nut` (`CmdBuildLayout`).
- **Scratchpad:** `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/st4/`
- **The rig:** another rig server is in use by the engineer on the default admin port. Don't stop it, and don't start a second server: the rig's admin port is fixed. Rig steps are listed in §4 for the orchestrator to run later. You only build and unit-test.

## 2. Changes

1. **JSON:** `JunctionLayoutToJson` writes `"stations":[{"x":..,"y":..,"axis":"X"|"Y","platforms":N,"length":L}]` when `layout.stations` is non-empty, and nothing when it's empty, so existing JSON is byte-identical.
   - Test: the existing JSON test still passes.
   - New test: a layout with one station block round-trips the expected substring.
2. **GS `CmdBuildLayout`:**
   - Build stations first, before tracks, signals and bridges: `GSRail.BuildRailStation(tile, direction, platforms, length, GSStation.STATION_NEW)`.
   - Direction: `GSRail.RAILTRACK_NE_SW` for axis X, `GSRail.RAILTRACK_NW_SE` for axis Y. Check the GS API in `src/script/api/script_rail.hpp` for the exact signature.
   - Failures go into `failed` the same way the others do.
   - Track entries that overlap station tiles must be skipped. Station tiles already carry the platform track, and building rail over them fails, so skip track entries whose tile lies inside a station block.
3. **`ottd` `layout_batches`:** include `"stations"` (it's small, so put it all in the first batch).
4. **Debug console command** (inside `#ifdef _DEBUG`, next to `junction_export`): `station_export <tile> <rotation 0-3> <mirror 0|1> <path> <platforms 2-8> [tl]`.
   - `MakeJunctionSpec` (L_R notation, given tl, default 4), then `GenerateStation(spec, platforms)`.
   - Orient the layout with `ApplyJunctionOrientation`.
   - The origin is the tile, treated as the station layout's north corner. There's no hub concept for stations in v1.
   - Write `{"origin":[x,y], ...layout json...}` and print `station_export ok origin=x,y size=WxH`, or `{"error":"NoLayout"}`.
   - `ApplyJunctionOrientation` must also rotate and mirror the `stations` blocks (origin and axis). Check it, and extend it if it only handles tracks, signals and bridges. Add a unit test: rotating a layout with a block by 1 swaps the axis and moves the origin consistently with the tracks.
5. **`ottd station-here X Y [--rot R] [--mirror] [--platforms N] [--tl T]`:** like `junction-here`, it calls `station_export`, reads the JSON and builds it at the origin. It prints `built=N failed=M`, plus each failure.

## 3. Tests and gates

1. Release `openttd_test`: the full suite, all passing.
2. The Debug `openttd` builds with no warnings in the touched files.
3. Commit: `Add: Stations export and build on the test rig`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 4. Rig check (don't run it; the orchestrator runs it after merging)
`ottd station-here 40 40 --platforms 3` builds with 0 failures.

## 5. Do not

- touch the junction generator or the junction GUI
- start or stop any rig server

## 6. Stop conditions

- The GS API has no way to build a multi-platform station in one call. Report what exists.
- Anything not covered.

## 7. Report

- the branch and commit
- red evidence
- the changes
- suite totals
- stops
- `git status --porcelain` and `git log -1`, both in the worktree

When you have reported, stop.
