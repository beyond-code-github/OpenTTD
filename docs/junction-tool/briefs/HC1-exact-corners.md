# HC1: the level solve's terraform from exact corners; ADJ and FLEX deleted (RECON-exact-corners.md; REVIEW Q4)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-hc1 <HEAD>` (given at launch). Never use git stash. Another agent works in /home/pete/OpenTTD/.claude/nodes2: never touch it. Use absolute paths.
- HC0 (docs/junction-tool/RECON-exact-corners.md: read it whole) proved on 2775 plans that the finished terraform for fixed track corners is, per free corner, clamp(ground, L, U) with L/U from L1 cones of the fixed corners (equal to PropagateJunctionTerraform's corners and cost on every plan Propagate accepts); and that using it as the descent's objective plans 2862 (+87, 0 lost), cheaper, T3 failures 0, 3.5 ms mean / 17.6 max per solve. Its experiment patch /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/hc0.patch (HC0_MODE/HC0_SLOPE switches) is the reference implementation, including the sparse clamp (equal to the dense clamp on 2862/2862).

## Read first (once each)
CODINGSTYLE.md (C++ part); the RECON; src/junction_levels.cpp; the patch's junction_levels.cpp parts; src/tests/junction_layout.cpp SimulateTerraformSteps (~:687) and the H2 step tests that use it.

## Changes (src/junction_levels.cpp; tests)
1. The descent's objective is the exact clamp cost of the full assignment (sparse clamp as in the patch), including the foundation-after check (a foundation tile whose corners the clamp moves must stay flat at its level or GetRailFoundation Leveled; otherwise the option is infeasible) and foundation_cost on moved foundation tiles as the patch charges it. The plan's terraform corners are the clamp's moved corners.
2. Delete `local_cost` (ADJ), the demanded/FLEX merge loops, the Propagate call inside SolveJunctionLevels, and the `SLOPE_RANGE = 2` loop (keep the descent's move range d = 1). PropagateJunctionTerraform stays for its other callers. No env switches, no modes, no probes.
3. If the patch needs a memo of tile choices for time, keep it file-local.

## Tests (record each red)
1. **Outer, `[realterrainsweep]`:** LEVELHASH re-pinned with evidence: planned 2775 -> expected ~2862, 0 plans lost (list any lost (fixture, offset) and STOP if > 0); HOVERPLAN planned >= 19 (report); HOVERNEAR planned >= 31 and `near_max < 100` ms (STOP and report the slowest 5 if not); T3 (JunctionRoutesHeightsOk on the terraformed terrain) and CheckJunctionRules clean on every planned fixture as today.
2. **`[hoverplan]` "the terraform plan is the game's":** for every fixture that plans at its offset (HOVERPLAN's set, not 3), MakeJunctionTerraformSteps then SimulateTerraformSteps (move it from the test file's static to a shared test helper only if the new test lives in another file; otherwise put the test beside it) reaches exactly the plan's corners, for 2 orientations. Red: none expected at HEAD (characterisation); record the HEAD result, then after.
3. **`[terraform]` "the clamp equals propagation where propagation succeeds":** 3 fixtures' plans: clamp corners and cost == PropagateJunctionTerraform's (the recon's check 1, kept as a test).
- `[topoprojsweep]` ridges 228/228 must hold; T3 fails 0.

## Stop conditions
- Any plan lost vs HEAD; HOVERNEAR max >= 100 ms; any T3 or rules failure; a step simulation that does not reach the plan's corners (report fixture, orientation, corner); anything not covered.

## Gates
- `tools/junction/jt fast build '[hoverplan]' 5`; `'[terraform]' 5`; `build-rel/openttd_test "[realterrainsweep]"`, `"[topoprojsweep]"`; `tools/junction/jt gate build-rel` (count, time; budget 120 s). TESTS.md rows.
- One commit: `Change: Junction level solve costs terraform exactly from corner cones; fitted spill terms removed`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus LEVELHASH/HOVERPLAN/HOVERNEAR lines before/after, level-solve ms mean/max, and test 2's result. When you have reported, stop.
