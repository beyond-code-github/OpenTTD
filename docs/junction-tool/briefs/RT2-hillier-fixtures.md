# RT2: regenerate the two "hilly" real-terrain maps with real hills

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes. First: `git checkout -B wt-rt2 <HEAD>` (given at launch). Never use git stash.
- RT1 (merged) made 48 fixtures with tools/junction/terrain_fixtures.py from three maps. Its report: the two "hilly" maps (seeds 101 temperate, 303 sub-arctic) used the rig's hilly config (terrain_type 5, custom_terrain_type 3) and came out nearly flat (max height 3, 93-96% flat tiles); the mountainous map (seed 202, terrain_type 3) reaches height 16 with 78% flat tiles. The engineer wants real hilly ground to test slopes and terraform.
- **Game processes:** the engineer's rig (ports 4077/4079) and GUI client may be running: never touch them. Use the script's own port/rundir as RT1 did; stop your server by its own pid.

## Read first
- tools/junction/terrain_fixtures.py; RECON-real-terrain.md; the game's terrain settings (src/table/settings/difficulty_settings.ini or world_settings.ini: terrain_type, variety distribution, smoothness, max height level, tgen).

## Changes
1. In the script only: seeds 101 and 303 use terrain_type "hilly" (the difficulty level value for Hilly, not custom), variety distribution high, smoothness rough, and max height level at least 30 (state the settings and values used); seed 202 unchanged.
2. Regenerate all 48 fixtures; commit them with the script. Rerun once to confirm byte-identical output.
3. Per map report: max height, % flat tiles, % of tiles with a slope, counts of Water/Blocked/Spannable tiles over its 16 snapshots. Target for the two hilly maps: under 70% flat tiles. If a setting combination cannot reach that, report the best one tried and stop before committing.
4. Run `build-rel/openttd_test "[realterrainsweep]"` and `"[realterrain]"`; report the REALTERRAIN lines (they will change; nothing asserts counts).
5. TESTS.md `[realterrainsweep]` row: the new solved counts.

## Gates
- `tools/junction/jt gate build-rel` (report the count).
- One commit: `Change: Junction real-terrain fixtures from hillier maps`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus the per-map table and the REALTERRAIN lines before/after. When you have reported, stop.
