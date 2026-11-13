# IG6: the in-game estimate and build check covers LL_RR, not just the T (IG2/IG3 for the hub the engineer builds)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-ig6 <HEAD>` (given at launch; branch wt-ts1 there is parked WIP: leave it). Never use git stash. Other agents work in /home/pete/OpenTTD/.claude/nodes2 and .claude/ng1: never touch them. Never start a game client or touch the engineer's game or rig servers. Use absolute paths.
- IG2/IG3/IG4 (`[junctiongame]`, src/tests/junction_game.cpp): the 48 real fixtures loaded into a game map, the GUI's path (PlanJunctionHoverNear + MakeJunctionTerraformSteps + EstimateJunctionTerraformed before the terraform), then the terraform with the game's command and the rail/bridge commands tested: 0 red rail tiles, 0 failing bridges, totals within foundation multiples. It runs the T only. The engineer builds LL_RR x L_R ("LL_RR + sideline" preset) in game.

## Changes (tests only)
1. The `[junctiongame]` fixture test runs for LL_RR x L_R too (MakeJunctionHubTopology from the two notations, as the window does), with the same checks: no red rail tile before the terraform; after the terraform with the game's command, 0 rail pieces and 0 bridges (and tunnels, if any) failing; the estimate total equals the post-terraform command total within the documented foundation multiples. Report planned count, red/failing counts and any total mismatch.
2. If anything fails for LL_RR, STOP and report it (fixture, tile, step, error): it is a real defect the engineer would hit, not something to relax.
3. TESTS.md row; keep the tag's Debug budget (10 s): if LL_RR pushes it over, move the LL_RR half to `[.identity]`-style hidden tag `[junctiongame][.slow]`... no: report the time and stop before re-tagging.

## Gates
- `tools/junction/jt fast build '[junctiongame]' 10`; `tools/junction/jt gate build-rel` (count, time).
- One commit: `Add: Junction in-game estimate and build check for the LL_RR hub`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus the LL_RR numbers. When you have reported, stop.
