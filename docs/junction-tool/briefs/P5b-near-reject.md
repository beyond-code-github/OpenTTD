# P5b: the near search rejects doomed offsets early (RECON-llrr-placement-fail.md)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes2 (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/nodes2 checkout -B wt2-p5b <HEAD>` (given at launch). Never use git stash. Other agents work in /home/pete/OpenTTD/.claude/nodes and .claude/ng1: never touch them. Never start a game client or touch the engineer's game or rig servers. Use absolute paths.
- P5 (docs/junction-tool/RECON-llrr-placement-fail.md, read it whole; patch /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/p5.patch on 7941320eb6): `JunctionPlacementNearReject` (bounds propagation per offset over the shared relations, the tile-group domains and the row-run avoid rule; shape recorded once in MakeJunctionPlacementShared) runs before the full placement at each near move (never at the cursor offset); 0 rejects among offsets that pass the X solve over 7344 LL_RR offsets; rejects 94.7% of X failures at ~16 us; two identity-preserving speed-ups (lazy group blocked-tile notes; holds_on_row keyed by row when classes_only). HOVERNEAR_LLRR mean ~35 ms (was ~94), fixtures over 100 ms 21 -> 3; T near mean 9.9 ms, max 56 ms; near plans and moved lines identical with the reject on/off.

## Changes
1. Apply p5.patch's src changes (resolve against HEAD: TU1-TU3 and MJ1/OF1/OF2 merged since 7941320eb6). The reject must treat a tunnel separation as a bridge does or skip it (TU2: a tunnel's portals are its heads; no span relation): say which and why.
2. Nothing else.

## Tests (record each red)
1. `[identity]` "the near reject never rejects a placeable offset": for T and LL_RR, all 48 fixtures x 153 offsets: wherever JunctionPlacementNearReject rejects, the full facts placement (as the near search runs it) fails; count rejects; 0 counterexamples. Report its time (identity budget 90 s; if over, move the T half to `[realterrainsweep]` and say so).
2. `[realterrainsweep]`: HOVERNEAR and HOVERNEAR_LLRR planned/moved unchanged; report means/maxes.
3. `tools/junction/jt identity build-rel`: both hashes unchanged.
4. Red: with the reject made deliberately unsound (e.g. drop one domain), test 1 reports a counterexample (record), then restore.

## Gates
- `tools/junction/jt fast build '[hoverplan]' 5`; `tools/junction/jt identity build-rel`; `tools/junction/jt gate build-rel` (count, time). TESTS.md rows.
- One commit: `Change: Junction near search rejects offsets the placement must refuse before placing them`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report with the numbers. When you have reported, stop.
