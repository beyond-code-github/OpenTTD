# IG3: bridge heads take the game's slopes; the span's ground stays at or below the heads (RECON-in-game-red-flat.md; engineer 2026-11-13)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree: given at launch (`<WT>`, Debug `build`, Release `build-rel`). First: `git -C <WT> checkout -B <branch> <HEAD>` (both given at launch). Never use git stash. Another agent works in the other worktree: never touch it. Never start a game client or touch the engineer's game or rig servers. Use absolute paths.
- The engineer (2026-11-13): "it's absolutely fine to have a bridge with one end on a slope and the other on flat ground. The net effect is then that the bridge ends flat." The game (src/tunnelbridge_cmd.cpp CheckBridgeSlope ~:209-227 and CmdBuildBridge ~:470-500): a head is valid on SLOPE_FLAT, on the one inclined slope rising toward the span (north head: SLOPE_NE for axis X, SLOPE_NW for Y; south head: SLOPE_SW / SLOPE_SE), or on any slope GetBridgeFoundation levels (a foundation, priced); the head's z is the tile z plus ApplyFoundationToSlope; both heads must end at the same z (BRIDGEHEADS_NOT_SAME_HEIGHT); every middle tile has GetTileMaxZ <= the head z (BRIDGE_TOO_LOW_FOR_TERRAIN) and the head z < tile z + max_bridge_height.
- The level solve (src/junction_levels.cpp) treats bridge heads as flat visits (`heads`), so a sloped head is always terraformed; nothing bounds the span's ground (IG1: 10 bridges fail BRIDGE_TOO_LOW_FOR_TERRAIN after the terraform). HC1 (da3e73a2b0) made terraform exact by corner cones (clamp(ground, L, U)); LS1's cone; HM1's GetRailFoundation rule.
- IG2 (if merged at launch) added the `[junctiongame]` fixture test that loads the 48 real fixtures into a game map and runs the GUI path; it counts the bridges still failing after the terraform.

## Read first (once each)
CODINGSTYLE.md (C++ part); docs/junction-tool/RECON-in-game-red-flat.md; src/tunnelbridge_cmd.cpp CheckBridgeSlope, GetBridgeFoundation, CmdBuildBridge's height checks; src/junction_levels.cpp (heads, Evaluate, the ramp rule, the clamp); docs/junction-tool/DESIGN-G1.md §15.

## Changes (src/junction_levels.cpp)
1. A head tile's options are the game's: flat at the head level h; the valid inclined slope (entry level h - 1 on the ground side, h at the span side: corners on the span side at h, the others at h - 1); a foundation where GetBridgeFoundation levels the tile's planned slope to h (priced foundation_cost). Both heads of a bridge are tied to one h (as today). The option is judged on the planned (clamped) corners, as HM1/HC1 judge foundations.
2. The inclined head is a one-level climb for the ramp rule (D219): it enters the ramp windows as a ramp tile.
3. Span clearance: every corner of every middle tile has an upper bound h (the shared head level) in the clamp (an upper-bound-only corner constraint, propagated by the cone as fixed corners are); and a lower bound so that h < tile z + max_bridge_height (read max_bridge_height from where the spec or settings carry it; if neither does, stop and report). Infeasible -> the solve fails with a reason naming the bridge and the tile.
4. Nothing else changes.

## Tests (record each red)
1. **`[hoverplan]` "a bridge head may sit on the slope rising to its span":** a hand-made snapshot: a bridge whose north head tile has the valid inclined slope and whose ground is otherwise at the levels the bridge needs: the plan does not terraform that head tile and the head's level is the slope's top. Red: HEAD terraforms it (record).
2. **`[hoverplan]` "the ground under a bridge stays at or below its heads":** a snapshot with a middle tile one level above the head's natural level: the plan lowers it (or raises both heads, whichever is cheaper) or fails with the bridge reason; never plans a span over higher ground. Red: HEAD plans over it (record).
3. **Outer:** the `[junctiongame]` fixture test (if IG2 merged): bridges failing after the terraform: 0 (was 10). `[realterrainsweep]`: LEVELHASH re-pinned with evidence (planned before/after; list lost plans and STOP if any plan is lost that the game accepts), HOVERPLAN >= 19, HOVERNEAR >= 31 and max < 100 ms; T3 and rules clean. `[topoprojsweep]` ridges 228/228.
4. TESTS.md rows.

## Stop conditions
- A plan lost that the game would build; HOVERNEAR max >= 100 ms; any T3/rules failure; anything not covered.

## Gates
- `tools/junction/jt fast build '[hoverplan]' 5`; `tools/junction/jt identity build-rel`; `build-rel/openttd_test "[realterrainsweep]"`, `"[topoprojsweep]"`, `"[junctiongame]"`; `tools/junction/jt gate build-rel` (count, time).
- One commit: `Change: Junction bridge heads take the game's slopes and the span's ground stays below them`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus counts before/after and what the engineer should see in game. When you have reported, stop.
