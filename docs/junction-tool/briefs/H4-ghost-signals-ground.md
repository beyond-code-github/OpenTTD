# H4: ghost signals at their track's level; terraformed ground shown on every planned tile (H3's report)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes2 (Debug `build`, Release `build-rel`). First: `git checkout -B wt2-h4 <HEAD>` (given at launch). Never use git stash. Another agent works in .claude/nodes: never touch it. Never start a game client or touch the engineer's game.
- H3 merged: the ghost draws each level tile at its solved level on its planned ground slope via `DrawOnCurrentTileAs(Slope, z, callback)` (src/viewport.cpp); bridge decks at head + 1. Two gaps H3 reported: (1) `DrawRailSignalGhost` uses the real tile's height, so ghost signals float or sink on terraformed tiles; (2) tiles the plan terraforms that carry no track (the propagation ring) are only outlined; their planned slope is not shown.

## Read first
- CODINGSTYLE.md; src/junction_ghost_draw.{h,cpp} (DrawJunctionGhost, DrawRailSignalGhost, the H3 changes); src/viewport.cpp DrawOnCurrentTileAs; src/junction_gui.cpp where JunctionGhost::Set gets steps and levels.

## Changes
1. Signal ghosts on a level tile are drawn inside the same DrawOnCurrentTileAs call as the track (planned slope and level z), so they sit on the planned track.
2. Every tile with a planned ground change (from the plan's steps) and no track draws the planned ground: a simple ghost of the planned slope at its planned z (reuse the existing ghost ground/tint sprite the outline uses, drawn through DrawOnCurrentTileAs); keep the blue outline.
3. A `[hoverplan]` test (no game): for 3 fixtures, every signal of the plan's layout lies on a tile that has a level (so it can be drawn at it), and every planned-ground tile has a slope and z from the plan (the data the ghost reads), equal to ApplyJunctionTerraform's tile.
4. TESTS.md row.

## Do not
- No pipeline change, no build-step change.

## Gates
- Inside nodes2: `tools/junction/jt fast build '[hoverplan]' 5`; `tools/junction/jt gate build-rel` (report the count); the `openttd` target builds Debug and Release.
- One commit: `Change: Junction hover ghost draws signals and planned ground at the planned heights`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus what the engineer should look at in game. When you have reported, stop.
