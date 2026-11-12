# F1: near-cursor frame search in the hover (D197; RECON-snap-negotiate E5; review queue item 3)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes2 (Debug `build`, Release `build-rel`). First: `git checkout -B wt2-f1 <HEAD>` (given at launch). Never use git stash. Another agent works in /home/pete/OpenTTD/.claude/nodes: never touch it. Never start a game client or touch the engineer's game (4077/4079). Use absolute paths; do not cd into other worktrees.
- D197 (DECISIONS.md, read it): the cursor spot comes first; a nearby offset is used only when the cursor spot has no layout at all, and it is shown as moved.
- N1 (docs/junction-tool/RECON-snap-negotiate.md §E5): with the same choices, moving the box by up to 8 columns / 4 rows plans 12 of the 29 real fixtures the hover fails today. A failing solve costs ~0.25 ms, a passing one ~9 ms.
- Today: PlanJunctionHover (src/junction_hover.{h,cpp}) plans at one offset. The GUI worker (src/junction_gui.cpp StartProjectionJob, ~:884-936) snapshots with margin 8 and calls it at {0,0}; ShowProjection passes distance 0 to ShowPlaced (whose `distance` parameter is how the old path shows "moved").
- Frames: terraform corners are in the snapshot's canonical frame (MakeJunctionTerraformSteps maps them; unaffected by the offset); layout and track_levels are in the placement frame; `JunctionProjectionResult::hub` is the layout position of the hovered tile, today {hub_x, 0}. Verify these three from the code before changing anything; if any is false, stop and report.

## Read first (once each, offset/limit)
CODINGSTYLE.md (C++ part); docs/junction-tool/TESTS.md (rows for [hoverplan], [realterrainsweep]); src/junction_hover.{h,cpp}; src/tests/junction_real_terrain.cpp (the "hover plan on real terrain" sweep, ~:260-330); src/junction_gui.cpp StartProjectionJob, ShowProjection, ShowPlaced's use of `distance`; src/junction_game.cpp MakeJunctionTerrain (margin).

## Tests (outermost first; record each red)
1. **`[realterrainsweep]` "hover plan on real terrain" gains a second pass:** for every fixture, also call PlanJunctionHoverNear at the same offset; print `HOVERNEAR fixtures=N planned=P moved=M ms_mean=.. ms_max=..` (moved = plans with moved != {0,0}); for every planned fixture run the existing checks (terraformed heights via JunctionRoutesHeightsOk, CheckJunctionRules clean) on the plan at offset + moved; `CHECK(near_planned >= <measured>)` with the measured count as the floor; `REQUIRE(near_max < 100.0)`. The existing HOVERPLAN line and its floor stay unchanged. Red: before the search exists (PlanJunctionHoverNear first written as a stub that returns PlanJunctionHover at the offset), near_planned equals the old count, below the floor you set after the search exists: record both numbers.
2. **`[hoverplan]` "the hover moves the box only when the cursor spot fails":** (a) a fixture that plans at its offset: PlanJunctionHoverNear returns moved {0,0} and the same layout (tracks and signals equal) as PlanJunctionHover; (b) a fixture that fails at its offset and plans moved (pick one from test 1's output): moved != {0,0}, PlanJunctionHover at offset + moved plans the same layout, and every offset earlier in the search order fails PlanJunctionHover. Red: the stub fails (b).

## Changes
1. src/junction_hover.h:
   - `static constexpr int JUNCTION_HOVER_NEAR_COLUMNS = 8; ///< How far the hover moves the box east or west when the cursor spot has no plan (D197; N1).`
   - `static constexpr int JUNCTION_HOVER_NEAR_ROWS = 4; ///< How far the hover moves the box north or south when the cursor spot has no plan (D197; N1).`
   - JunctionHoverPlan gains `TileIndexDiffC moved{}; ///< How far the placement was moved from the asked offset; {0, 0} at the cursor spot (D197).`
   - `JunctionHoverPlan PlanJunctionHoverNear(const JunctionGraphSeed &seed, const JunctionSpec &spec, const JunctionTerrain &terrain, TileIndexDiffC offset);` with a doc comment: plans at offset; only when that fails, tries offset + d for every d with |d.x| <= COLUMNS, |d.y| <= ROWS, d != {0,0}, in order of |d.x| + |d.y|, then |d.y|, then d.x, then d.y (ascending), returning the first that plans with `moved = d`; when none plans, returns the cursor spot's plan (its `why`).
2. src/junction_hover.cpp: implement it by calling PlanJunctionHover; nothing else in the pipeline changes.
3. src/junction_gui.cpp:
   - StartProjectionJob: snapshot margin `8 + std::max(JUNCTION_HOVER_NEAR_COLUMNS, JUNCTION_HOVER_NEAR_ROWS)`; the worker calls PlanJunctionHoverNear(…, {0, 0}); `result.hub = {hub_x - plan.moved.x, -plan.moved.y}`.
   - JunctionProjectionResult gains `int distance = 0; ///< How far the plan was moved from the hovered spot (D197), 0 at the spot.` set to max(|moved.x|, |moved.y|); ShowProjection passes it to ShowPlaced instead of 0.
   - If MakeJunctionTerraformSteps or the levels mapping need the offset after all (the frame check in State), stop and report.
4. TESTS.md rows for both tests (and the [realterrainsweep] time).

## Do not
- No change to PlanJunctionHover, the placement, levels, render or signals. No cost ranking across offsets (D197: nearest wins). No new status string. No time-budget mechanism.

## Stop conditions
- A frame statement in State is false; near_max >= 100 ms (report the distribution and the slowest fixtures); a planned moved fixture fails the heights or rules checks (fixture, offset, why); anything not covered.

## Gates
- Inside nodes2: `tools/junction/jt fast build '[hoverplan]' 5`; `build-rel/openttd_test "[realterrainsweep]"` (both lines); `tools/junction/jt gate build-rel` (report the count); the `openttd` target builds Debug and Release.
- One commit: `Feature: Junction hover moves the box to a nearby spot when the cursor spot has no plan`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report (references read, commit, red evidence, types/members added, suites, stops, `git status --porcelain`, `git log -1`), plus HOVERPLAN and HOVERNEAR lines, the moved distances, and what the engineer should look at in game. When you have reported, stop.

## Amendment 1 (orchestrator, after the stop on near_max 483 ms)
1. Work invariant across offsets is done once: split PlanJunctionHover's body into a file-local `static JunctionHoverPlan PlanJunctionHoverAt(const JunctionGraphSeed &seed, const JunctionSpec &spec, const JunctionTerrain &terrain, const JunctionTrackOrder &order, const JunctionFacts &facts, TileIndexDiffC offset)`; PlanJunctionHover builds the order (with its error return as today) and the facts, then calls it; PlanJunctionHoverNear builds them once and calls it per offset. Behaviour and counts identical.
2. Measure again. If near_max is still >= 100 ms, do not change anything else: report, for one failing offset of temperate-mountainous-6 and the passing offset of temperate-hilly-15, the time of facts construction, order, the placement (Y solve, X solve, levels/terraform if reached) and render+signals (temporary timers, removed before you stop), plus the number of offsets tried per slow fixture; then stop at a clean state with your edits uncommitted.
3. If under 100 ms: continue with the rest of the brief (GUI, [hoverplan] test, TESTS.md, gates, commit).

## Amendment 2 (orchestrator, after LS1 merged)
- LS1 (level solve ~4x faster, identical plans, cone reject) is merged. P1 measured HOVERNEAR with it at planned 31, ms_max ~90.
- Start: `git -C /home/pete/OpenTTD/.claude/nodes2 checkout -B wt2-f1b <HEAD>` then `git -C /home/pete/OpenTTD/.claude/nodes2 cherry-pick --no-commit 043c159963` (F1's WIP: PlanJunctionHoverNear with the hoisted PlanJunctionHoverAt, constants, `moved`, the HOVERNEAR sweep pass with a placeholder floor). Resolve conflicts by keeping both sides' tests.
- Then the rest of the brief: floor = measured near_planned; `REQUIRE(near_max < 100.0)`; the GUI changes; the `[hoverplan]` test; TESTS.md; gates; the one commit. Also report the wide gate's time (it now carries ES1's and LS1's sweeps; budget 120 s).
- If near_max >= 100 ms: stop and report the slowest 5 with their offsets tried; change nothing else.
