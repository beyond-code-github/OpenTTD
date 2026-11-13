# TU4: the level solve plans tunnel portals and the ground over a tunnel (D220; RECON-tunnel-crossings.md step TU4)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1 (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/ng1 checkout -B wt-tu4 <HEAD>` (given at launch). Never use git stash. Other agents work in /home/pete/OpenTTD/.claude/nodes and .claude/nodes2: never touch them. Never start a game client or touch the engineer's game or rig servers. Use absolute paths.
- TU1-TU3 merged (c79aec76ed): a tunnel separation is placed (portals = heads) and renders as a `JunctionBridge{tunnel = true}`, signalled, checker clean (flat, no levels). TU0 (docs/junction-tool/RECON-tunnel-crossings.md, read it whole): game rules (src/tunnelbridge_cmd.cpp CmdBuildTunnel ~:641-800): the entry portal is on a RAW inclined slope (no foundation), the exit is the first tile past it whose lowest corner equals the entry's z (a middle tile at the floor's z ends the tunnel early, so every middle tile, and the crosser's tile, must be at least one level above the tunnel), the game fixes up the exit slope by terraform; the train in a portal sits at the tile's z (no climb on the portal). The level solve (src/junction_levels.cpp) skips tunnels today (~:345, ~:524). IG3/IG4 made bridge heads and bridge ramps the game's; HC1/P4b the exact clamp cost (incremental).

## Read first (once each)
CODINGSTYLE.md (C++ part); the RECON; src/tunnelbridge_cmd.cpp CmdBuildTunnel; src/junction_levels.cpp (heads, IG3's head options, IG4's ramp profile, the clamp and its incremental update, where tunnels are skipped); src/tests/junction_game.cpp the `[junctiongame]` harness (IG2/IG4: real game map, commands).

## Changes (src/junction_levels.cpp)
1. A tunnel's two portals are visits with exactly one option each: the raw inclined slope rising away from the tunnel along its axis, at tunnel level L (lower corners at L, upper at L+1), no foundation option; both portals tied to one L.
2. Every middle tile of the tunnel (between the portals, on its line) gets a lower bound L+1 on all its corners in the clamp (a lower-bound-only corner constraint, as IG3 added upper-bound-only ones for bridge spans); the crosser track tiles there are then at least L+1 by the same bound.
3. The ramp rule: a portal is no climb for the tunnel's route (the game's z on a portal is the tile's z); the middle tiles are level at L for the route (they are inside the tunnel). Say how the windows count them.
4. Nothing else; a tunnel only exists in tests until TU5.

## Tests (record each red)
1. `[hoverplan]` "the level solve plans a tunnel": a hand-made snapshot with TU3's tunnel T (T seed, single bridge set to tunnel) on flat ground: the solve terraforms the portals' slopes and raises the ground over the middle (cost > 0), every portal's planned slope is the raw incline, every middle corner >= L+1; T3 (JunctionRoutesHeightsOk) accepts it. Red: today the solve ignores the tunnel (record what it plans).
2. `[junctiongame]` "a planned tunnel builds": the same plan on a game map: terraform steps then the game's CmdBuildTunnel (test and execute) succeed and the tunnel's far end is the planned portal. Red: none expected beyond test 1's; if the command fails, STOP and report the error (the model is wrong).
3. Identity: `tools/junction/jt identity build-rel` (both hashes unchanged: no seed has a tunnel); `[topoprojsweep]` 228/228.
4. TESTS.md rows.

## Gates
- `tools/junction/jt fast build '[hoverplan]' 5`; `tools/junction/jt fast build '[junctiongame]' 10`; `tools/junction/jt identity build-rel`; `tools/junction/jt gate build-rel` (count, time).
- One commit: `Add: Junction level solve plans tunnel portals and the ground over a tunnel`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus the tunnel's planned cost and the game command result. When you have reported, stop.
