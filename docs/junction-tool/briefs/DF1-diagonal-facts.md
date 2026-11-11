# DF1: a free diagonal's interior tiles meet the facts in the solve

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (persistent; Debug `build`, Release `build-rel`, incremental builds). First: `git checkout -B wt-df1 <HEAD>` (HEAD given at launch). You are the only writer there.
- Why: docs/junction-tool/DIAG-node-shortfalls.md Q2 and AUDIT-rule-coverage.md (RC1, reachable gap 4): SolveJunctionNodes builds `diagonal_rasters` (src/junction_nodes.cpp, after step 2d "The occupancy chain") and never uses them, so a free diagonal's interior (half) tiles are never checked against the facts; only the search's post-solve "fits" loop catches a blocked one, and it cannot repair it. After the Y snap a diagonal's extra steps m are known (NodeDiagonalSteps reads only Y values), so its raster is fixed relative to its first point p, and p's X class is the only variable that moves it (DESIGN-N3 §6).

## Read first (once each, offset/limit)
- docs/junction-tool/DESIGN-N3.md §6 (free diagonals, two-phase solve).
- src/junction_nodes.cpp: NodeDiagonalSteps and NodeDiagonalRaster (~:685-698), RenderJunctionNodeTracks (the diagonal loop: interior pieces i = 1 .. size-2 at `raster.pieces[i].tile + (shift(from.cls), shift(from.ycls))`), SolveJunctionNodes step 3 "The pieces of every class fit the facts" (~:1025-1072).
- src/junction_nodes.h JunctionNodeTrackFits, JunctionNodeFailure.
- The existing SolveJunctionNodes tests in src/tests/junction_nodes.cpp (grep "SolveJunctionNodes(" and pick the nearest flat-T solve test as the pattern for building tpl, facts and offset).

## Tests (outermost first)
1. Red, src/tests/junction_nodes.cpp, tag `[nodes]`: `"a free diagonal's interior tiles fit the facts"`. Build the template from the test-spec T as the existing flat-T solve test does; `REQUIRE(!tpl.diagonals.empty())`. Take `tpl.diagonals[0]`, its raster at the flat length (`diag.halves`; the raster helper is static, so rasterise in the test with `RasterizeJunctionRoute({p.tile, p.trackdir, std::string(diag.halves - 1, 'F')})` exactly as NodeDiagonalRaster does), and its interior piece i = 1. Make the terrain flat as in the pattern test and set that one tile (in working coordinates: layout tile + offset) Blocked. Solve. Assert: either `solve.failure` is set, or for every track of `*solve.layout` (normalised: map back with the anchor shift as the pattern test does, or, simpler, assert on the working tiles you can compute) the tile fits — concretely: `REQUIRE(solve.layout.has_value())` is NOT required; instead assert that no track of the rendered layout sits on the blocked working tile. Pick whichever of these two forms the pattern test makes straightforward and say which in the report. Expected before the change: the layout has a half track on the blocked tile (the assertion fails naming that tile).
2. `[nodes]` and `[nodespeed]` stay green unchanged.

## Changes
- src/junction_nodes.cpp, SolveJunctionNodes step 3, inside the per-value lambda/loop for class c (where `fits` is computed): after the Y-free-link column check, add: for each diagonal index d with `tpl.points[tpl.links[tpl.diagonals[d].link].from].cls == c`, using `diagonal_rasters[d]` (already built just above step 3): if its `error` is non-empty, `fits = false`; otherwise for i = 1 .. pieces.size()-2: `x = pieces[i].tile.x + v - tpl.flat[c] + offset.x`, `y = pieces[i].tile.y + shift_y(p.ycls) + offset.y`, and `fits` requires `JunctionNodeTrackFits(facts, x, y, TrackBits{TrackdirToTrack(pieces[i].trackdir)})`.
- The failure stays `Kind::Piece` with `cls = c` (the class with no fitting value). Update the step-3 comment to say it covers the free diagonals' interior tiles from their rasters at the known length.
- Nothing else. The search's post-solve "fits" loop stays.

## Do not
- Do not change JunctionNodeFailure, the search, the render, or any other step of the solve. Do not move the raster construction.

## Stop conditions
- `tpl.diagonals` is empty for the test-spec T (then report which spec has diagonals, e.g. 196 km/h, and stop).
- Any existing [nodes]/[nodespeed]/[tmapnear]/[nodeoracle] test goes red.

## Gates
- `tools/junction/jt fast build nodes 5`, `tools/junction/jt fast build-rel nodes 1`, `tools/junction/jt fast build-rel nodespeed 5`, `tools/junction/jt fast build-rel tmapnear 5`, `tools/junction/jt gate build-rel`.
- Commit: `Fix: Junction node solve checks free diagonal interiors against the facts`, body ends with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
References read; commits; red evidence (failure message); members added/removed (expect none); suite results with times, including the ORACLE line printed by the gate's [nodeoracle] test; stops; `git status --porcelain` and `git log -1`. When you have reported, stop.
