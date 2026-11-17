# S2b: shape program: crossing direction from the topology, prefer right-angle crossings

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes; build only `openttd_test` in `build-rel`, never `openttd`. Code read with offset/limit. Scratch: `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/S2b/`.

## State
Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, clean at `{HEAD}` (S3b: each variant scores the default and the program shape). Read `/home/pete/OpenTTD/docs/junction-tool/wip/DIAG-ll2shape.md`, DESIGN-tracks §12 (judgement calls), `planar_shape.{h,cpp}`, `src/tests/junction/planar/planar_shape.cpp`, and in `planar_paths.cpp` how the default decides an under piece's heading relative to the over (which side the under crosses from) and `CheckPlanarShape`.

## Findings to act on
- The program's crossing row `H_under - H_over = a + 4q`, a in [1,3], lets the under line cross the over from either side; the topology fixes the side (default shapes always match it). Flipped sides gave LL²'s "infeasible with topology sides" (o49t0f1s0 X4/X5, o2t0/o2t4 X4).
- 45° and 90° crossings cost the same; a 90° crossing turned 45° made o4*f0 base infeasible (bridge rows) and o5t* +625 area.

## Changes
1. **Rule (derived):** for each channel crossing, the sign s in {+1, -1} of (H_under - H_over) mod 8 (s = +1 when it is in 1..3, -1 when in 5..7) is fixed from the topology. Derive s from the topology's crossing order exactly as the default path code determines it (read it; if the default takes it from something other than the topology and the over choice, stop and report what). Row: H_under - H_over = s * a + 8 q, a in [1, 3]. `CheckPlanarShape` gains the same check (the under crosses from the topology's side), with its reason text naming "side".
2. **Cost (opinion):** add w * |a - 2| per crossing to the objective (via a = 2 + u - v, u, v >= 0), w = 1 (one 45° crossing step costs as much as one 45° turn). Constant `SHAPE_CROSSING_ANGLE_COST = 1` with a doc comment stating it is an opinion (right-angle crossings need the shortest bridge), not a rule. `PlanarShapeTurns` unchanged (turns only); `SolvePlanarShape`'s `turns` out-param stays turns only.
3. Tests: in planar_paths tests, `planar paths: a shape crossing from the wrong side is refused` (T2 default, flip one under heading to the mirrored side, reason contains "side"; red before the check). In planar_shape tests the existing guards stay; add `planar shape: the program keeps every crossing's side` (all variants of T2, T3; result's sides equal the default's). LL² variant of it in the [slow] case.
4. CLAUDE.md: the two facts (side rule; right-angle preference as an opinion with its weight). DESIGN-tracks is updated by the orchestrator, not you.

## Gates
Build; `jt` gate (report time); [planarshape] incl. slow; T2/T3 mapped [slow] tests. pv T2, T3, LL² (201 tags in `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/ll2-tags.txt`), `-j 6`, before (HEAD) vs after, both default mode (scoring both shapes) and `PLANAR_SHAPE=program`: per hub solved, best area, better/same/worse, median/max ms; in program mode list any variant still failing with its reason. Stop before committing if default-mode best area gets worse on any hub or a solving variant fails.
Commit: `Change: Junction shape program keeps crossing sides and prefers right-angle crossings` + body + `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
Commit; red evidence; suites, gate time; pv tables; stops; `git status --porcelain`, `git log -1`. Under 20 lines. When you have reported, stop.
