# IG4: the ramp rule counts the climbs the game's trains feel at bridge heads and across bridges (REVIEW Q4 ramp-rule gap (a); IG3's report; D219)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-ig4 <HEAD>` (given at launch). Never use git stash. Another agent works in /home/pete/OpenTTD/.claude/nodes2 (src/junction_gui.cpp): never touch it. Never start a game client or touch the engineer's game or rig servers.
- D219 (DECISIONS.md): zero speed loss on ramps under realistic acceleration: within tl consecutive tiles a route may rise at most JunctionRampLevels(spec) levels. The game's trains feel every z change: GroundVehicle::UpdateZPosition (src/ground_vehicle.hpp ~:130-215) sets GoingUp whenever the track's middle z is above the vehicle's z, which includes the ramp onto a FLAT bridge head (deck at head z + 1) and the ramp off it; an INCLINED head (IG3: the slope falling toward the span) carries the track at the deck's height with no climb on the head.
- IG3 (5ed40cb65e): heads take the game's slopes (flat, inclined, foundation); two heads' levels may differ by 1. The ramp rule (src/junction_levels.cpp, review: "windows containing a bridge middle are skipped, not counted") does not look across a bridge.

## Read first (once each)
CODINGSTYLE.md (C++ part); src/ground_vehicle.hpp UpdateZPosition and GetSlopePixelZ for bridge heads (src/tunnelbridge_cmd.cpp GetSlopePixelZ_TunnelBridge / the bridge ramp z); src/junction_levels.cpp the ramp rule (windows, visits, heads, middles); docs/junction-tool/REVIEW-opus-since-terrain-placement.md Q4 ramp-rule gaps.

## Step 0 (findings, no code change)
From the game code: the z profile a train sees across a bridge with (a) two flat heads, (b) one inclined and one flat head, (c) two inclined heads: per tile, the rise (in levels) on the head tiles and the middle tiles. Report it as a small table with file:line. If the game's train sees no climb on a flat head (contrary to State), stop and report.

## Changes (src/junction_levels.cpp)
1. A route's visits across a bridge enter the ramp rule as the train sees them: the near head tile carries the step 0 rise (a flat head: +1 into the deck; an inclined head: 0), each middle tile is a visit at the deck level (0 rise), the far head carries the step 0 fall/rise. Windows are no longer skipped when they contain a middle; they count these visits.
2. Nothing else (no change to gap (b) train-length units or (c) uneven weight).

## Tests (record each red)
1. **`[hoverplan]` "the ramp rule sees the climb onto a bridge":** a hand-made snapshot where a route climbs ramp_levels levels on track within tl tiles before a flat head: the plan must not place that (it levels the approach, uses the inclined head, or fails with the ramp-rule reason). Red: HEAD plans it (record).
2. **`[junctiongame]` "an inclined bridge head builds":** on a game map, a bridge with one head on the valid inclined slope (as IG3's option) and the other flat at the level IG3 ties them to: the game's CmdBuildBridge (test and execute) succeeds. Red: none expected (characterisation of IG3's reading of CheckBridgeSlope); if it fails, STOP and report: IG3's head model is wrong.
3. **Outer:** `[realterrainsweep]` LEVELHASH re-pinned with evidence (planned before/after; list lost plans; a lost plan is acceptable only when its route climbed onto a bridge beyond ramp_levels: show one), HOVERPLAN >= 19 (report), HOVERNEAR >= 31 (report any drop with the cause) and max < 100 ms; `[junctiongame]` 0 bridges failing; ridges 228/228 (report if fewer and why).
4. TESTS.md rows.

## Stop conditions
- Step 0 contradicts State; test 2 fails; a plan lost for a reason other than a bridge climb; HOVERNEAR max >= 100 ms; anything not covered.

## Gates
- `tools/junction/jt fast build '[hoverplan]' 5`, `'[junctiongame]' 10`; `tools/junction/jt identity build-rel`; `build-rel/openttd_test "[realterrainsweep]"`, `"[topoprojsweep]"`; `tools/junction/jt gate build-rel` (count, time).
- One commit: `Fix: Junction ramp rule counts the climb onto bridges`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus step 0's table and counts before/after. When you have reported, stop.
