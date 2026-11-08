# MX1: map area export and hotspot finder (console commands)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Work **only** in the worktree `/home/pete/OpenTTD/.claude/play`, which is a separate checkout. Another agent is building in `/home/pete/OpenTTD`, so don't touch that one.
- First, in the play worktree, run `git checkout --detach <head given in your launch prompt>`. It is detached at an older commit and has a Release build dir, `build-play`. Build in `build-play` only.
- Commit in the play worktree, on detached HEAD. The orchestrator will cherry-pick your commit onto `feature/junction-tool`, so report its hash.
- Decision D128 (in `docs/junction-tool/DECISIONS.md`): read maps with the game's own loader and conversions, not a separate parser. The same map-reading code will later serve hub upgrades (D125).

## Read first (once each, with offset/limit)
- `CODINGSTYLE.md` (in full).
- `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/savread/savread.py`, its JSON-writing code: field names and values per tile kind. Your JSON must use the same schema, so the existing analysis scripts work on it.
- `src/console_cmds.cpp`: the `junction_export` and `station_export` commands (pattern, registration, arguments).
- `src/tests/junction_build.cpp`: tests that `Map::Allocate` and lay rail on a small map. Copy their setup.
- Map accessors: `src/rail_map.h`, `src/signal_type.h`, `src/tunnelbridge_map.h`, `src/bridge_map.h`, `src/station_map.h`, `src/road_map.h` (level crossings), `src/tile_map.h`.

## New code
`src/junction_map_export.h` / `.cpp`, registered in `src/CMakeLists.txt` next to the other junction files.

```cpp
/** One ranked map window for finding hubs and big stations. */
struct JunctionHotspot {
	uint x0, y0, x1, y1;
	uint presignals; ///< Entry, exit and combo signals.
	uint pbs; ///< Path signals, both kinds.
	uint bridge_heads; ///< Rail bridge heads.
	uint station_tiles; ///< Rail station tiles (waypoints excluded).
	double score;
	std::vector<StationID> stations; ///< Rail stations with tiles inside.
};

std::string JunctionMapExportJson(uint x0, uint y0, uint x1, uint y1);
std::vector<JunctionHotspot> JunctionMapHotspots(uint top_k, uint window);
```
- **`JunctionMapExportJson`:** for every tile in the inclusive box that is rail, rail tunnel/bridge, rail station or waypoint, or a road tile with a level crossing, write one tile record with savread's field names and values. Top level: `map_size`, `box`, `orientation` (copy savread's orientation text) and `tiles`.
  - Signals: per signal, give track, trackdir, exit_edge, type (block/entry/exit/combo/pbs/pbs_oneway), variant (electric/semaphore) and state.
  - Bridge and tunnel heads: give far_end, using `GetOtherTunnelBridgeEnd`.
  - Stations: give station_index, station_name (resolved with the game's own string formatting for the station), axis, waypoint, gfx and station_type.
  - Owner as savread writes it.
  - If savread has a field you can't fill from the live map, leave it out and list it in the report. Don't invent values.
- **`JunctionMapHotspots`:**
  - Windows of `window`×`window`, stride `window/2`, clipped to the map.
  - `score = 3*presignals + 2*bridge_heads + station_tiles/4.0 + pbs/2.0`.
  - Return the top_k windows by score, ties broken by smaller y0 then x0, overlapping windows allowed.

## Console commands (`src/console_cmds.cpp`, registered like `junction_export`)
- `map_export <x0> <y0> <x1> <y1> <path>`: writes the JSON to path, then prints `map_export ok tiles=<n>`.
- `map_export_station <station id> <margin> <path>`: the box is the station's rail tiles' bounding box expanded by margin and clipped to the map.
- `map_hotspots <k> [window=48]`: one line per hotspot with its box, counts, score and station ids with names.

Help text follows the existing commands' style.

## Tests (`src/tests/junction_map_export.cpp`, tag `[junction][mapexport]`; register in `src/tests/CMakeLists.txt`)
1. **Outer:** "map_export JSON of a hand-built area". On a 64×64 map, set up as `junction_build.cpp` does:
   - an X-axis rail line with a block signal and a one-way PBS signal;
   - a rail bridge;
   - a level crossing, if a helper makes it easy; otherwise skip it and say so.
   Export a box around them, parse the JSON with the JSON library the junction code already uses (find it in `junction_layout` JSON code), and assert each tile's fields:
   - kinds, tracks;
   - signal type, trackdir, exit_edge;
   - bridge far_end.
   Red: a stub returning `"{}"`.
2. "Hotspots rank windows by score": two areas on the map, one with more pre-signals. The top hotspot is the busier window, with the expected counts. Red: the stub returns empty.

## Manual validation (required; report what you saw)
1. Start a dedicated server from the play build with the engineer's save, sending commands on stdin:
   ```
   printf 'map_export_station 110 40 /tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/mapx/fledhattan.json\nquit\n' | build-play/openttd -D -g "/home/pete/.local/share/openttd/save/Buborough Bay Transport, 2243-02-19.sav"
   ```
   - Use a copy of the save in the scratchpad.
   - If stdin commands don't work in dedicated mode, find the supported way (for example `-x`, a script file via `exec`, or the rig's `tools/ottd/ottd rcon`) and report it.
   - Don't use port 3979 if the rig server is running. Check with `pgrep -af openttd`; use another port or config if needed.
2. Compare it tile by tile with `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/savread/out/fledhattan_east.json` (station 110, margin 40). Use a small script in `scratchpad/mapx/`. Report the number of tiles compared and every field difference, grouped. Differences in fields savread could only approximate are fine but must be explained.
3. Load `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/psgsaves/dl/PSG_186.sav` (savegame v141) and `PSG_219.sav` (v163) the same way. Run `map_hotspots 10` and export the top 3 windows of each to `scratchpad/mapx/psg186_*.json` and `psg219_*.json`. Report:
   - whether the saves loaded (missing NewGRFs: what the game did);
   - the hotspot lines;
   - the files written.

## Do not
- Kill or restart the running `build-play/openttd` processes (the engineer's client and the rig server use that binary; relinking is fine, killing is not).
- Touch `/home/pete/OpenTTD` (the main worktree).
- Change savegame format, game state, or any existing command's behaviour.
- Add network commands.

## Stop conditions
- Old saves can't load in a dedicated server without a NewGRF the system lacks, and the game refuses: report the message and continue with the other parts.
- Anything would need game-state changes.

## Gates
1. Build `build-play` targets `openttd` and `openttd_test`, with output to files. No errors and no new warnings.
2. `./openttd_test "[mapexport]"` passes.
3. Run the full `build-play/openttd_test` with output to a file.
4. Commit in the play worktree: `Add: Junction map area export and hotspot console commands`, with the body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
- References read
- Commit hash in the play worktree
- Red evidence
- Types, functions, commands and files added
- Fledhattan comparison results
- PSG load and hotspot results, with file paths
- Suite results
- Stops, and anything left undone
- State: `git -C /home/pete/OpenTTD/.claude/play status --porcelain` and `log -1`

When you have reported, stop.
