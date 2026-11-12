# H3: the ghost draws track at its solved levels (slopes, ramps, bridge decks) (D218; H2's report)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes2 (Debug `build`, Release `build-rel`). First: `git checkout -B wt2-h3 <HEAD>` (given at launch); then, before any change, build both and run `tools/junction/jt gate build-rel` once (H2 and C5b were merged together: record the count as the baseline). Never use git stash. Another agent works in .claude/nodes: never touch it. Never start a game client or touch the engineer's game (4077/4079).
- H2 merged: the hover runs PlanJunctionHover and shows the layout with a blue outline on terraformed tiles. Two gaps H2 reported: (1) the ghost draws sprites only on flat tiles, so on slopes the layout is mostly invisible; (2) bridge deck height uses `estimate.height`, 0 when the plane check is off, so decks may be drawn at the wrong z.
- The level solve (src/junction_levels.cpp) knows every track tile's level and shape (flat, foundation, ramp) but the placement exposes only the terraform plan and height_cost.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); TESTS.md; src/junction_levels.{h,cpp} (its outputs); src/junction_topology.h JunctionTrackPlacement (terraform, height_cost); src/junction_hover.{h,cpp}; src/junction_ghost_draw.{h,cpp} and JunctionGhost::Set; the H2 changes in src/junction_gui.cpp; how the old path's ghost picks a tile's z and sprite.

## Changes
1. JunctionTrackPlacement gains `std::vector<std::pair<TileIndexDiffC, int8_t>> track_levels; ///< The level solve's track level per track tile (placement frame), for the ghost and the build check.` filled by the level solve (terraform mode).
2. JunctionHoverPlan carries the levels (mapped as the layout is), and the ghost draws each track tile at its level: on the terraformed ground's slope (after the plan) the game's foundation/ramp sprite choice as the old ghost does for flat tiles at a plane; if the existing ghost drawing cannot draw sloped track, draw the track sprite at the level's z with the tile's terraformed slope outline (say which).
3. Bridge decks at their head level + 1 from the levels, not estimate.height.
4. Tests (no game): `[hoverplan]` "the hover plan carries a level for every track tile" on 3 fixtures: every layout track tile has exactly one level; levels agree with ApplyJunctionTerraform's terrain (flat tiles: z_min == z_max == level, or a foundation with z_max == level; ramp tiles: the incline along the piece).
5. TESTS.md rows.

## Do not
- No pipeline behaviour change; no build-step change.

## Stop conditions
- The ghost code has no way to draw at an arbitrary z (report what it can); anything not covered.

## Gates
- Inside nodes2: `tools/junction/jt fast build '[hoverplan]' 5`; `build-rel/openttd_test "[realterrainsweep]"`; `tools/junction/jt gate build-rel` (report before/after); the `openttd` target builds Debug and Release.
- One commit: `Change: Junction hover ghost draws track at its solved levels`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus what the engineer should look at in game. When you have reported, stop.

## Amendment 1 (orchestrator, after the stop; engineer away, reversible)
1. **The prio element is levelled and terraformed for (a pipeline fix, now in scope).** The level solve takes the prio element's footprint tiles (C5b's footprint occupants: per main-main row, step 0 and steps 1..even_prio on the row side) as track tiles: each flat (never a ramp: joins are diagonal pieces), and each at the level of the main-line tile beside it in the same column (`==`). Their bits for the per-tile cost/fit are the bits the signal pass draws there (X plus the join halves; read the signal pass for the exact bits, or use X | both halves as the worst case for fits; say which). The terraform plan then covers them. Test: the brief's "a level for every track tile" holds with no exclusion, and T3 on the terraformed terrain still passes on every planned fixture; REALTERRAIN/HOVERPLAN counts reported before/after (they may drop: then report which fixtures and why).
2. **Viewport helper approved:** add the small src/viewport.cpp helper that lets the ghost draw a tile at a given z and slope (reset the per-tile foundation state, set `_cur_ti.z`/`tileh` for the ghost draw, restore after); `DrawJunctionGhost` may take a non-const TileInfo. No `const_cast` of `_cur_ti`.
3. Resume from your saved patch; then changes 2-5 and all gates.
