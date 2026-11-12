# HM1: the level solve's levelled foundation is the game's (REVIEW-opus-since-terrain-placement Q4 "a model/game mismatch that will bite")

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-hm1 <HEAD>` (given at launch). Branch wt-r7b holds a parked WIP: leave it alone. Never use git stash. Another agent works in /home/pete/OpenTTD/.claude/nodes2: never touch it. Use absolute paths.
- Evaluate (src/junction_levels.cpp ~:77-110) offers a levelled foundation for a flat visit when the tile is a gentle slope with z_max == level, excluding only an axis piece along a two-corner incline (`along`). The game (GetRailFoundation, src/rail_cmd.cpp ~:310-365) builds a lone X (or Y) on a slope with ONE corner raised as Foundation::InclinedX/Y: the track climbs, it is not levelled. So the model can plan a flat track the game builds as a ramp. Other bit sets (curves, X plus join halves, HORZ/VERT) have their own game answers (Leveled, a half-tile foundation, Invalid).
- JunctionTerrainTile has `slope` (canonical frame, steep bit kept). Visit bits are in the same canonical frame as the slope (verify; if not, stop and report).
- LS1 (b0ebcb86c3) pinned `LEVELHASH` over 48 fixtures x 153 offsets (src/tests/junction_real_terrain.cpp) and the HOVERPLAN floor 19.

## Read first (once each, offset/limit)
CODINGSTYLE.md (C++ part); src/junction_levels.cpp Evaluate and its callers; src/rail_cmd.cpp GetRailFoundation; how src/junction_facts.cpp calls GetRailFoundation (Fits) and how T3 (JunctionRoutesHeightsOk) computes a track's z (both are the pattern to follow).

## Step 0 (findings, no code change)
On a hand-made snapshot (a straight X run over flat ground with one tile whose N corner (or whichever corner is not on the run's own incline) is raised by 1, others at the run's level - 1... build it so that z_max == the run's level and only one corner is raised): what does the level solve choose for that tile at HEAD (foundation flat at z_max?), what does GetRailFoundation say for its slope and bits, and does T3 (JunctionRoutesHeightsOk on the unterraformed terrain) accept the plan? Report.

## Change
In Evaluate, the levelled-foundation option is offered exactly when `GetRailFoundation(tile->slope, bits) == Foundation::Leveled` for the visit's bits on that tile (the bits converted to TrackBits as junction_facts.cpp does), replacing the `along` test and the `!tile->steep && z_min < z_max` part that GetRailFoundation covers (keep `z_max == level`, build_on_slopes, Fits and the cost comparison). One comment line citing rail_cmd.cpp. Nothing else in the solve changes (no new inclined-foundation ramp option: noted for later).

## Tests (record each red)
1. **`[hoverplan]` "the level solve's foundation is the game's":** step 0's snapshot: the solve does not plan that tile as a levelled foundation (it terraforms it or ramps or fails with a reason), and T3 accepts the plan if it plans. Red: HEAD plans the levelled foundation (say whether T3 caught it).
2. **`[hoverplan]` exhaustive:** for every non-steep, non-flat Slope and each of these bit sets: X, Y, each single half/curve piece, X plus each join half used by the prio element, HORZ, VERT: Evaluate's levelled-foundation option exists iff GetRailFoundation(slope, bits) == Foundation::Leveled (via a 1-tile snapshot and the public solve entry, or through an existing test seam; if neither reaches Evaluate without a new seam, stop and report).
3. **Outer, `[realterrainsweep]`:** run LEVELHASH and HOVERPLAN. If the hash moves: list every (fixture, offset) whose plan changed and, for a sample of 5, the tile(s) that had a levelled foundation the game would not build; re-pin the hash and planned count only with that evidence. HOVERPLAN planned may not drop below 19; if it does, stop and report which fixtures and why.

## Stop conditions
- Bits and slope frames differ; HOVERPLAN < 19; a changed plan not explained by the rule; anything not covered.

## Gates
- `tools/junction/jt fast build '[hoverplan]' 5`; `build-rel/openttd_test "[realterrainsweep]"` and `"[topoprojsweep]"` (ridges 228/228 must hold; report); `tools/junction/jt gate build-rel` (count and time). TESTS.md rows.
- One commit: `Fix: Junction level solve offers a levelled foundation only where the game builds one`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus step 0's findings and the LEVELHASH/HOVERPLAN lines before/after. When you have reported, stop.
