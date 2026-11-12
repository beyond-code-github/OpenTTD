# H1: the hover plan on the new pipeline, terraform as build steps, a terraformed estimate (no GUI yet) (RECON-hover-switch §7-§8; DESIGN-G1 §14.7 T5, §15; D197, D218, D219)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes2 (Debug `build`, Release `build-rel`). First: `git checkout -B wt2-h1 <HEAD>` (given at launch). Never use git stash. Another agent works in .claude/nodes: never touch it.
- RECON-hover-switch.md (read it whole): the hover runs the old node path; the new pipeline (seed -> cut -> facts placement with terraform -> render -> signals; plan + height_cost) is only in tests (src/tests/junction_real_terrain.cpp ~:215-242). This step builds the game-free half of the switch: one function the hover will call, terraform plans as map-coordinate build steps under every rotation/mirror, and a cost estimate including terraform. H2 wires the GUI.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); TESTS.md; RECON-hover-switch.md; DESIGN-G1.md §15.
- src/tests/junction_real_terrain.cpp (the call chain); src/junction_game.cpp MakeJunctionTerrain, OrientJunctionVector, the frame; src/junction_layout.{h,cpp} MakeJunctionBuildSteps, the build-step variant, EstimateJunction; src/junction_terraform.{h,cpp}; src/terraform_cmd.cpp CmdTerraformLand (one corner, one level per command).

## Tests (record each red against stubs)
1. **Outer `[realterrainsweep]` "hover plan on real terrain"**: PlanJunctionHover on every fixture with the T seed at the box-centred offset: where it returns a layout, the plan's terraformed terrain passes T3 (as Z2e's check), signals present, CheckJunctionRules clean; where it fails, `why` is non-empty. Print `HOVERPLAN fixtures= planned= failed= ms_mean= ms_max=` (wall time per call, Release). REQUIRE ms_max < 100. Red: stub returns nothing.
2. **`[junction]` "terraform steps: every orientation"**: a small canonical terrain with a plan (raise one corner by 2 near a slope); for each of the 8 rotation/mirror combinations: map the terrain to a map frame the way MakeJunctionTerrain does, make the steps, simulate them one by one on a corner-height grid (each step = one corner +/-1 as CmdTerraformLand does), and the result equals ApplyJunctionTerraform's corners mapped the same way. Red: stub.
3. **`[junction]` "the terraformed estimate adds the height cost"**: EstimateJunctionTerraformed(layout, plan, height_cost, pricing stub) = the rail/signal/bridge estimate + height_cost, with no plane check. Red: stub.

## Changes
1. `struct JunctionHoverPlan { std::optional<JunctionLayout> layout; JunctionTerraform terraform; Money height_cost = 0; std::string why; };` and `JunctionHoverPlan PlanJunctionHover(const JunctionGraphSeed &seed, const JunctionSpec &spec, const JunctionTerrain &terrain, TileIndexDiffC offset);` in a new src/junction_hover.{h,cpp} (CMakeLists): the test chain (cut placement's choices, facts placement with terraform = true and the terrain, render, signals). The seed is const and shared (thread-safe use: say what you checked).
2. A build-step kind for terraform: `struct JunctionTerraformStep { TileIndex tile; Corner corner; bool raise; };` added to the build-step variant, and `std::vector<JunctionTerraformStep> MakeJunctionTerraformSteps(const JunctionTerraform &plan, const JunctionTerrain &terrain, <the orientation inputs MakeJunctionTerrain uses>);` mapping canonical corners to map tiles/corners under rotation and mirror (lattice-point inverse), one step per corner-level move, ordered so each step is legal (the propagation order of PropagateJunctionTerraform or lowest-first for raises). Place it beside MakeJunctionBuildSteps; the GUI does not post them yet.
3. `Money EstimateJunctionTerraformed(...)` beside EstimateJunction: no single-plane check; adds height_cost.
4. TESTS.md rows.

## Do not
- No GUI, no job/thread change, no ghost change, no command posting (H2). No change to placement, levels, terraform model.

## Stop conditions
- ms_max >= 100 (report the slowest fixtures and where the time goes); the orientation mapping is ambiguous for a corner; anything not covered.

## Gates
- Inside nodes2: `tools/junction/jt fast build '[junction]' 5` (or the tags the new tests use), `build-rel/openttd_test "[realterrainsweep]"` (to a file), `tools/junction/jt gate build-rel` (report the count); the `openttd` target builds.
- One commit: `Add: Junction hover plan on the projection pipeline with terraform build steps`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus the HOVERPLAN line and the thread-safety reading. When you have reported, stop.
