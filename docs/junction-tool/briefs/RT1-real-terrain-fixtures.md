# RT1: real-terrain fixtures from generated maps, and a placement oracle on them (engineer: "pick random locations of real map terrain for testing rather than flat land or artificial ridges")

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git checkout -B wt-rt1 <HEAD>` (given at launch). Never use git stash.
- RECON-real-terrain.md (read it whole): JunctionTerrain is filled from the live map by MakeJunctionTerrainRect (src/junction_game.cpp ~:385); the rig starts seeded dedicated servers (tools/ottd/ottd_core/server.py, `--hilly`); no terrain file format exists; nlohmann json is available (see src/junction_map_export.cpp). Recommendation adopted: dump once from real games, check in fixtures, replay in tests.
- **Game processes:** the engineer's rig (ports 4077/4079) and his GUI client may be running. Never stop, restart or connect to them. Your server uses its own port (4177/4179 or the next free), its own scratch rundir and config, and you stop it when done (by its own pid, recorded at start; never pkill/pgrep -f patterns).

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); TESTS.md; RECON-real-terrain.md; src/junction_terrain.h (every member of JunctionTerrain and JunctionTerrainTile); src/junction_game.cpp MakeJunctionTerrainRect and FillJunctionTerrainCell; src/console_cmds.cpp `junction_export` (:322-442) as the pattern for a console command; src/junction_map_export.cpp (json use); tools/ottd/ottd_core/server.py (how the rig starts a seeded server and sends console commands).
- src/tests/junction_game.cpp the `[placeoracle]` test (ForEachOracleTerrain, the offset, the per-terrain checks).

## Tests (record each red)
1. **Inner `[realterrain]` "real terrain: json round trip"**: a hand-made JunctionTerrain with every member set to non-default values (every tile class, slopes, inclines, steep, costs): JunctionTerrainFromJson(JunctionTerrainToJson(t)) equals t member by member. Red: stub returns nullopt.
2. **Outer, `[realterrainsweep]` "placement on real terrain"**: every fixture file under tools/junction/terrain/ loads; per fixture the T graph seed is placed by the facts overload with its own choices at the offset that puts the seed's box centred in the snapshot. Where it solves: render, JunctionRoutesHeightsOk, CheckJunctionRules (T topology): REQUIRE heights ok and no violation (INFO fixture and each). Print `REALTERRAIN fixtures= solved= failed= micros=` and, per failure kind, the first reason rule counts. Red: no fixtures (record).

## Changes
1. **src/junction_terrain_json.{h,cpp}** (add to src/CMakeLists.txt next to junction_terrain):
```cpp
/** A terrain snapshot as JSON (test fixtures from real maps): every member of JunctionTerrain and each tile's members, tiles as one compact array per row. */
std::string JunctionTerrainToJson(const JunctionTerrain &terrain);
/** The snapshot back; nullopt with the reason in error on a malformed file. */
std::optional<JunctionTerrain> JunctionTerrainFromJson(std::string_view json, std::string *error);
```
   Tiles: per row a JSON array of per-tile arrays of integers in a fixed member order (documented in the .cpp), so a 80x40 snapshot stays small.
2. **Console command** `junction_terrain_dump <x> <y> <w> <h> <file>` in src/console_cmds.cpp beside junction_export: MakeJunctionTerrainRect over the rectangle, JunctionTerrainToJson, write the file; help text as junction_export's.
3. **tools/junction/terrain_fixtures.py**: starts a dedicated server with the rig's helpers on its own port and rundir, for each of 3 maps: (seed 101, temperate, hilly, 256x256, towns normal), (seed 202, temperate, mountainous terrain type, 256x256), (seed 303, sub-arctic, hilly, 256x256); per map 16 rectangles of 80x40 at positions from a fixed-seed Python RNG, kept away from the map edge by 2 tiles; dumps each with the console command to tools/junction/terrain/<map>-<n>.json; stops the server. Deterministic: rerunning writes identical files.
4. **The fixtures** (48 files) committed. If they total more than 5 MB, STOP and report the size.
5. **src/tests/junction_real_terrain.cpp** (register in src/tests/CMakeLists.txt): tests 1 and 2; the fixture directory found relative to the source tree the way other tests find tools/ottd/layouts (read how; if none does, use a compile definition and say so).
6. TESTS.md: `[realterrain]` row (fast), `[realterrainsweep]` row (gate tag; Release time; budget 10 s).

## Do not
- No change to placement, facts, the node path, the rig scripts' behaviour (add only the new script), the engineer's config or running games.

## Stop conditions
- The server cannot be started on a separate port/rundir with the rig helpers; the console command cannot reach MakeJunctionTerrainRect; fixtures over 5 MB; test 2 shows a solved placement with heights or violations (report each: fixture, tile, rule) — a finding, do not change placement; anything not covered.

## Gates
- `tools/junction/jt fast build '[realterrain]' 5`; `build-rel/openttd_test "[realterrainsweep]"` (to a file); `tools/junction/jt gate build-rel` (report the count). `openttd` (game target) builds in Release.
- One commit: `Add: Junction real-terrain fixtures from generated maps and a placement sweep on them`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus the REALTERRAIN line and failure-reason counts, fixtures' total size, the server port/rundir used and that it was stopped. When you have reported, stop.
