# P6b: the level descent stays inside the cone's level ranges (RECON-corner-join-reject.md)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes2 (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/nodes2 checkout -B wt2-p6b <HEAD>` (given at launch). Never use git stash. Other agents work in /home/pete/OpenTTD/.claude/nodes and .claude/ng1: never touch them. Never start a game client or touch the engineer's game or rig servers. Use absolute paths.
- P6 (docs/junction-tool/RECON-corner-join-reject.md, read it whole; patch /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/p6.patch: its `P6_REACH` fix is the change; everything else in it is diagnostic and must not be committed): the cone filter in src/junction_levels.cpp computes per-tile level ranges, checks they meet, then undoes them (`pos.Undo(cone_mark)`); the start levels and the descent never see those ranges, so the descent walks outside what the cone ruled out and reports "corners ... cannot be joined by one terraform" where a plan exists. Keeping the ranges and clamping each group's descent range to `pos.Lo`/`pos.Up`: T planned 2919 -> 3064, LL_RR 1180 -> 1601, 0 plans lost, 17 changed cost (13 cheaper), all plans re-place/render/T3/rules clean; HOVERNEAR_LLRR mean 34.9 -> 11.0 ms, max 548 -> 37.1 ms, 0 fixtures over 100 ms; T unchanged.

## Changes
1. The P6_REACH change only (keep the cone's ranges for the start levels and the descent; clamp each group's range), without the env switch; a comment citing P6.
2. Nothing else.

## Tests
1. `tools/junction/jt identity build-rel`: both LEVELHASH tests move BY RULE: re-pin them only after checking, with a dump at launch HEAD vs after (the P6 harness method), that 0 plans are lost, every changed offset either newly plans or changes cost, and every plan passes T3 + CheckJunctionRules (report the counts). Pin the new planned counts and hashes.
2. `[realterrainsweep]`: HOVERPLAN / HOVERNEAR for T and LL_RR (planned may only rise; floors raised to the new counts); report times. If the LL_RR near max is now under 100 ms on every fixture, add `REQUIRE(near_max < 100.0)` to the LL_RR pass too.
3. `[junctiongamesweep]` (IG6, if merged) or `[junctiongame]`: 0 red, 0 failing after terraform for T and LL_RR.
4. `[topoprojsweep]` ridges 228/228.
5. TESTS.md rows.

## Gates
- `tools/junction/jt fast build '[hoverplan]' 5`; `tools/junction/jt identity build-rel`; `tools/junction/jt gate build-rel` (count, time).
- One commit: `Fix: Junction level descent stays inside the ranges the ground cone allows`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report with the numbers. When you have reported, stop.
