# Z2f: prio element tiles may ramp with the main line where they carry only straight track (H3's report; D218, D219)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes2 (Debug `build`, Release `build-rel`). First: `git checkout -B wt2-z2f <HEAD>` (given at launch). Never use git stash. Another agent works in .claude/nodes: never touch it.
- H3 merged: the level solve takes the prio element's tiles (SignalJunctionTrackRoutes' `prio_beside`: each prio tile and its main-line tile) as flat one-tile visits tied `==` to the main-line tile, with the exact drawn bits. Cost: terraform solves 17 -> 13 and hover plans 18 -> 14 on the real fixtures (arctic-hilly-13, arctic-hilly-3, temperate-mountainous-1: a prio-tile corner fixed flat conflicts with propagation; temperate-hilly-4: a propagated corner reaches a Blocked tile). Main-line tiles that receive a join half are no longer rampable (correct: a switch tile is flat).
- The prio row's tiles alternate X-only and X-plus-join-half (e.g. bits 1 and 9). An X-only tile is an axis piece along the row: like any axis tile it may ramp, provided it ramps exactly as the main-line tile beside it does (both rows climb together, so the joins between them stay on flat tiles).

## Changes
1. In the level solve, a prio tile whose drawn bits are a lone axis piece along the row becomes a rampable visit: its entry and exit levels equal the main-line tile's entry and exit levels beside it (`==` on both), instead of one flat level. Prio tiles with join halves stay flat (and so their main-line neighbours, as today).
2. Tests: `[realterrainsweep]` counts before/after (terraform solved, hover planned); raise the floors in src/tests/junction_real_terrain.cpp to the new counts if higher (never lower); `[hoverplan]` "a level for every track tile" still passes; T3 on terraformed terrain passes on every solved fixture.
3. TESTS.md rows.

## Do not
- No other level-solve change; no placement change.

## Stop conditions
- Any solved fixture failing T3 (report: fixture, tile, why); counts lower than today; anything not covered.

## Gates
- Inside nodes2: `tools/junction/jt fast build '[hoverplan]' 5`; `build-rel/openttd_test "[realterrainsweep]"` and the ridge sweep (in `[topoprojsweep]`: 227/228 must hold); `tools/junction/jt gate build-rel` (report the count).
- One commit: `Change: Junction prio element tiles ramp with the main line where they carry only straight track`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus counts before/after and which fixtures came back. When you have reported, stop.
