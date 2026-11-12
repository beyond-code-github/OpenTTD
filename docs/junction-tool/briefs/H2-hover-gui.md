# H2: the in-game hover uses the projection pipeline (toggle), shows terraform, builds terraform first (RECON-hover-switch §7; D197, D218, D219)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes2 (Debug `build`, Release `build-rel`). First: `git checkout -B wt2-h2 <HEAD>` (given at launch). Never use git stash. Another agent works in .claude/nodes: never touch it.
- **Game processes:** the engineer's rig (ports 4077/4079) and his GUI client may be running. Never start a GUI client, never touch his processes. This step is verified by tests and by building the game; the in-game check waits for the engineer (say so in the report).
- H1 merged: `PlanJunctionHover(seed, spec, terrain, offset)` (src/junction_hover.{h,cpp}) returns layout + terraform plan + height_cost + why; `MakeJunctionTerraformSteps(plan, terrain, anchor, rotation, mirrored, hub_x)` (src/junction_layout.cpp) gives map-coordinate one-level corner steps, legal in order; `EstimateJunctionTerraformed`; `GameJunctionStepPoster::Post` has a placeholder branch for JunctionTerraformStep returning false. RECON-hover-switch.md (read it whole) describes the GUI job model, frame, ghost, build sequencer.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ and GUI parts); TESTS.md; RECON-hover-switch.md; src/junction_gui.cpp (the hover job, cache key, SizingStatus, the status line, the ghost set, the build start); src/junction_game.cpp (MakeJunctionTerrain, the frame, GameJunctionStepPoster); src/junction_ghost_draw.{h,cpp}; src/terraform_cmd.cpp (CmdTerraformLand's parameters); src/junction_hover.{h,cpp}; the H1 tests.

## Changes
1. **Toggle:** a junction-tool GUI option (a checkbox or toggle button in the junction window, following the window's existing controls; string in lang/english.txt) "Projection (terraform)": off = today's path, on = the new path. Default ON (the engineer wants to see the new pipeline). Stored where the window keeps its other options.
2. **New hover job (on):** same rest delay and thread model as today; the job snapshots terrain on the main thread with MakeJunctionTerrain over the seed box + margin 8 (frame: hub_x = box width / 2, as RECON §3 proposes), then on the worker calls PlanJunctionHover with a cached T graph seed per SpecKey (made once on the main thread or under the existing cache's lock: say which). Topology: the T (LL_RR selection is the picker step, not this one).
3. **Ghost:** the layout as today; tiles in the terraform plan tinted (reuse an existing ghost colour/overlay mechanism; if none, a simple per-tile highlight); the status line shows the estimate from EstimateJunctionTerraformed including the terraform cost, or the plan's `why` when it fails (never a silent fallback).
4. **Build:** the build steps start with MakeJunctionTerraformSteps(...) and post CmdTerraformLand per step (replace the placeholder branch), then the existing rail/bridge/signal steps; a failed terraform step goes through the sequencer's existing Retry/Cancel path.
5. **D219:** the new path keeps the realistic-acceleration refusal (SizingStatus).
6. **Tests (no game):** (a) the job's pure part (snapshot -> plan -> steps) on 3 real-terrain fixtures under rotations 0-3 and mirror: the steps' map tiles lie inside the snapshot and their simulated result equals the plan (reuse H1's simulator); (b) fix H1's test tags: the real-terrain sweep must not carry the fast `[hoverplan]` tag (keep it in `[realterrainsweep]` only; `[hoverplan]` fast under 1 s); (c) strengthen H1's T3 check to the terraformed terrain: ApplyJunctionTerraform then the heights check on the rendered routes (as Z2e does).
7. TESTS.md rows.

## Do not
- No topology picker, no LL_RR in the GUI, no edits/A*, no change to the pipeline functions. Do not remove the old path.

## Stop conditions
- The job model cannot snapshot the bigger terrain without exceeding the old path's main-thread time by more than 3x (report the measured times); CmdTerraformLand's parameters do not match one-corner steps; anything not covered.

## Gates
- Inside nodes2: `tools/junction/jt fast build '[hoverplan]' 5`, `build-rel/openttd_test "[realterrainsweep]"` (to a file), `tools/junction/jt gate build-rel` (report the count); the `openttd` target builds in Debug and Release.
- One commit: `Add: Junction hover on the projection pipeline with terraform (toggle)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus: the toggle's place in the window, the snapshot time (main thread) old vs new if measurable in tests, what the engineer should check in game (a short list), and that no game process was touched. When you have reported, stop.
