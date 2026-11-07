# Brief D78 step T5: rig check on hilly maps

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`.
- **What exists already:** D78 T1–T4 (terrain snapshot, router and generator on terrain, hover worker). The rig is in `tools/ottd/` (see its README). The rig runs `build/openttd` (Debug) via `OTTD_RIG_BIN`.
- **Goal:** prove that generation on real terrain gives layouts the game can build. Every generated layout must build with 0 failures.
- **The engineer watches rig runs:** print the join command `build/openttd -n 127.0.0.1` when the server starts. Don't wait.

## 2. Read first

- `src/console_cmds.cpp`: the `ConResetTile` pattern (l.293) and its registration (l.2761), inside `#ifdef _DEBUG`
- `src/junction_game.h/.cpp`: `MakeJunctionSpec`, `MakeJunctionTerrain`
- `src/junction_layout.h`: `ApplyJunctionOrientation`, `OrientJunctionOffset`
- `src/tests/junction_design.cpp`: `WriteLayoutJson`
- `tools/ottd/ottd`, `tools/ottd/ottd_core/server.py` (the CFG template and `-G` seed), `tools/ottd/README.md`

## 3. Changes

### 3.1 JSON writer moves to game code
- Add `std::string JunctionLayoutToJson(const JunctionLayout &layout);` to `junction_layout.h/.cpp`, with the same output as the test's `WriteLayoutJson` today (without depots).
- `WriteLayoutJson` calls it, and appends depots when it has them, so the output is unchanged. Check that the existing exported JSON files are byte-identical when regenerated.

### 3.2 Debug console command `junction_export` (`console_cmds.cpp`, inside `#ifdef _DEBUG`)
Usage: `junction_export <tile> <rotation 0-3> <mirror 0|1> <path> [ml] [branch] [tl]`, defaulting to `L_R L_R 4`.

1. Parse the arguments, using `ParseInteger` as `ConResetTile` does.
2. Use rail type `RAILTYPE_RAIL`. Run as company 0 if it exists (a `Backup<CompanyID>` of `_current_company`, the pattern other console code uses). If there is no company, print an error.
3. Parse the notations with `ParseJunctionNotation`.
4. `spec = MakeJunctionSpec(...)`.
5. effective mirror = mirror XOR (road side is left).
6. `terrain = MakeJunctionTerrain(tile, rotation, effective mirror)`, then `GenerateJunction(spec, &terrain)`.
7. On success, compute `oriented` and `origin` exactly as the T4 window does. Write `{"origin":[x,y],` plus the body of `JunctionLayoutToJson(oriented)` (merge them into one JSON object) to the path. On failure, write `{"error":"<enum name>"}`.
8. Print one line: `junction_export ok origin=x,y size=WxH`, or the error.

### 3.3 Rig
- **`ottd start --hilly`:** in the generated config use `custom_terrain_type = 3`, `variety = 2`, `tgen_smoothness = 1`, `quantity_sea_lakes = 2` and `number_towns = 4`. Keep the seed override. Without `--hilly`, nothing changes.
- **`ottd junction-here X Y [--rot R] [--mirror] [--ml L_R] [--branch L_R] [--tl N]`:**
  - rcon `junction_export` with the tile index (`y * map_x + x`) and a path in the run dir;
  - read the JSON;
  - on success, `build-layout` it at the given origin and print `built=N failed=M`; on an error, print it.
  - Exit codes: 0 if built OK, 3 for a generator error, 1 for build failures.
- **`ottd junction-scan --count K [--seed S]`:**
  - pick K tiles (random with seed S, at least 20 tiles from the map edge), with rotation random 0–3;
  - run `junction-here` on each;
  - print a line per try, then the totals: generated, no layout, built OK, build failures.
  - It must not stop on a failure.

The map is 128×128, so `map_x` comes from the config (2^7). Read it from the config rather than hard-coding it.

### 3.4 Run
1. `ottd start --hilly`, then `ottd company`, then `ottd junction-scan --count 12 --seed 1`.
2. Print the join command when the server starts.
3. Report the per-try lines and the totals. Building and the failure count are enough for this step; no trains.

## 4. Tests

- A unit test for `JunctionLayoutToJson`: the output for `MakeTestJunctionLayout()` matches a hand-written expected string.
- The existing exports are byte-identical (see 3.1).

## 5. Do not

- change the generator, router, checker or GUI
- add release-build console commands

## 6. Stop conditions

- **Build failures happen on generated layouts.** That is the finding this step exists to surface. Don't fix anything: report each failure with its error text, its tile, and that tile's slope and class from a `junction_export` snapshot (add a debug print if needed, then remove it). Then stop.
- `Backup<CompanyID>` doesn't fit.
- Anything not covered.

## 7. Gates

1. `cmake --build build --target openttd openttd_test > /tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/t5_build.log 2>&1`. There must be no errors or warnings in the files you touched.
2. Full suite, all passing.
3. The rig run in §3.4.

Commit message: `Add: Junction export console command and hilly rig scan`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 8. Report

- commit
- the scan output
- suite totals
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
