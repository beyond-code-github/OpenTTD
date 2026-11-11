# PF1: the reserved prio row meets the tile facts in the solve

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (persistent; Debug `build`, Release `build-rel`). First: `git checkout -B wt-pf1 <HEAD>` (given at launch). You are the only writer there. `tools/junction/jt` builds openttd_test before running.
- Measured (docs/junction-tool/DIAG-fits-row2.md): all 4 post-solve "fits" rejections in [nodeoracle] (seeds 100, 113, 135, 136) are the prio row of the merge at working (20,3): PlaceJunctionSignals lays it after the solve on Blocked tiles. `tpl.prio_rows` (DESIGN-N3 §8) is used only by the occupancy chain (AddJunctionNodeOccupancy); the step-3 fact filter and the Y snap never check its tiles. D193/D194: correct by construction.

## Read first (once each, offset/limit)
- docs/junction-tool/DIAG-fits-row2.md; DESIGN-N3.md §8 and §10; docs/junction-tool/TESTS.md.
- src/junction_nodes.h JunctionNodePrioRow; src/junction_nodes.cpp: where prio_rows are built (AddPathDiffs rule 3), AddJunctionNodeOccupancy's prio element (the reserved range: x_M - span .. x_M on row(M) + side), SolveJunctionNodes step 2b (`y_fits`, `x_values`) and step 3 (per-class filter, `join_meets`, `bits_at`).
- src/junction_signal_pass.cpp PlacePrio (grep): which tracks it lays on the prio row (the X run and the half track(s) joining the main line) so you can state the bits per tile exactly.

## Design (settled)
- The reserved tiles of a prio row are part of the merge point's X class (they move with x_M) and its Y class (row(M) + side moves with row(M)).
- Step 3, class c == the merge point's cls, value v: every reserved tile (x_M - span .. x_M at row(M) + side, in working coordinates as step 3 converts) must Fit every track set PlacePrio can lay on it: the lone X track, and X together with each half track PlacePrio can add on that tile (read PlacePrio; list the sets in a comment). And consecutive reserved tiles must meet (EdgeZ of the lone X at the shared edge, as NodeJoinMeets compares), and the reserved tile at x_M must meet the main line's merge tile at the edge they share through the joining half (use the bits PlacePrio lays there with the merge tile's flat bits).
- Y snap (y_fits (i)): the same condition is part of the merge's rigid group check, so a Y value whose prio row has no x is rejected.
- Failure kinds stay as they are (Piece in step 3, Height in the Y snap).

## Tests (record each red)
1. Outer: [nodeoracle]: `CHECK(fits == 0)` added beside the heights check. Red now: fits=4.
2. Inner `[nodes]`: `"a blocked prio row moves its merge"`: the test-spec T template on flat terrain; block one working tile inside the merge's reserved prio row at the flat solution (compute it from tpl.prio_rows and the flat values); solve; REQUIRE a layout or a failure; if a layout: finish it as SearchJunctionNodes does (the signal pass) and CHECK no track lies on the blocked tile. Red now: the prio track lands on it.
3. `[nodes]`, `[nodespeed]`, `[tmapnear]`, `[facts]` stay green; if an existing expectation flips, STOP and report.

## Do not
- Do not change PlacePrio, the occupancy chain, the search, the oracle, or the facts.

## Stop conditions
- PlacePrio's laid track sets cannot be stated per tile from the code (then report what it lays and stop).
- Test 1 still red: report the remaining fits lines and stop without committing.
- `[nodes]` Release over 1 s, `[nodeoracle]` Release over 5 s.

## Gates
`tools/junction/jt fast build "[nodes]" 5`, `tools/junction/jt fast build-rel "[nodes]" 1`, `tools/junction/jt fast build-rel "[nodespeed]" 5`, `tools/junction/jt fast build-rel "[tmapnear]" 5`, `tools/junction/jt gate build-rel`. Commit: `Fix: Junction node solve checks the reserved prio row against the facts`, body with the ORACLE line before and after, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
References read; commit; red evidence; the track sets per reserved tile you derived from PlacePrio; members added/removed; suites with times; ORACLE lines; stops; `git status --porcelain` and `git log -1`. When you have reported, stop.
