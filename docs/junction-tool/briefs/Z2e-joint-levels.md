# Z2e: the joint level solve replaces the per-edge DP (RECON-joint-levels; DESIGN-G1 §15.3; D218, D219)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes2 (Debug `build`, Release `build-rel`). First: `git checkout -B wt2-z2e <HEAD>` (given at launch). Never use git stash. Another agent works in .claude/nodes: never touch it. Game processes: the engineer's rig (4077/4079) and client may be running; never touch them; the fixture script uses its own ports.
- RECON-joint-levels.md (read it whole) and its patch /home/pete/OpenTTD/docs/junction-tool/wip/z2d-joint-levels.patch: one level per track tile in a JunctionPositions, continuity per route, the ramp window (D219) per route, bridge crossers <= heads, no fixed ends, greedy start + coordinate descent, plus ADJ (a shared corner of two track tiles is one height; a ramp tile's exit-side corners are the next tile's variable), FLEX (a foundation tile accepts any corner from its ground up to its level), SLOPE2 (track corners within Manhattan distance d differ by at most d): 17/18 fixtures and 227/228 ridges (margin 8), all passing T3; 7 ms mean, 12.9 ms max Release.
- Found by the recon: the fixture JSONs have no `terraform_cost` (Z2a added the member after RT1), so the sweep ran with free terraform.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); TESTS.md; RECON-joint-levels.md; the patch; src/junction_levels.{h,cpp}; src/junction_terrain_json.cpp; tools/junction/terrain_fixtures.py; src/tests/junction_real_terrain.cpp and the Z2c tests.

## Tests (record each red)
1. **Outer `[realterrainsweep]`**: the terraform line requires solved >= 17 of the hard-solved fixtures (at the regenerated fixtures' real prices) and, for every solved one, T3 on the terraformed terrain (SamePlacement against the terraform-mode placement + JunctionRoutesHeightsOk). Red: Z2c's per-edge DP gives far fewer (record).
2. **`[topoproj]` (or a new `[levelsweep]` gate tag if over budget) "levels: ridges across the T"**: the recon's ridge scan (ridge columns and rows over the box, heights 1-4, margin 8): solved >= 227 of 228, every solved one passes T3. Put the scan in a sweep tag; keep one fast representative case in `[topoproj]`.
3. **`[realterrain]` json round trip** covers `terraform_cost` (and `foundation_cost` if missing).
- Every existing test green; Z2c's tests keep passing (ramp spacing, flat ground no plan, ridge row cut).

## Changes
1. junction_levels.cpp: the recon's final model (ADJ + FLEX + SLOPE2) replaces the per-edge DP; no switch; Z2c's DP code removed. Keep the public interface. The ramp rule stays the window form `L(t+tl) - L(t) <= k` per route.
2. Recon follow-ups, simplest form: ground-following tiles' corners bounded by their ground (unary limits), corners outside the snapshot free to move (not fixed), so the spill conflicts the recon names disappear or are reported.
3. Terrain JSON carries `terraform_cost` (and every JunctionTerrain member not yet carried); regenerate the 48 fixtures with the script (byte-identical rerun), commit them.
4. TESTS.md rows (new tag if added).

## Do not
- No ramp tokens in programs, no edits, no A*, no in-game build of terraform. No change to placement relations.

## Stop conditions
- Test 1 under 17 or test 2 under 227 after the changes (report the failures grouped); any T3 failure on a solved case (finding); the fixture script cannot run on its own port; anything not covered.

## Gates
- Inside nodes2: `tools/junction/jt fast build '[topology]' 5`, `'[topoproj]' 5`, `'[terraform]' 5`, `'[realterrain]' 5` (jt takes the build dir as its argument); `build-rel/openttd_test` with `"[realterrainsweep]"`, `"[placeoracle]"`, the new sweep tag (to files); `tools/junction/jt gate build-rel` (report the count).
- One commit: `Change: Junction joint level solve over every track tile (D218)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus the REALTERRAIN lines, the ridge counts, cost (where both old and new solve), timing mean/max. When you have reported, stop.
