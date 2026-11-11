# HN1: a Height failure names its part and its tiles (DESIGN-N3 §11, trigger)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (persistent; Debug `build`, Release `build-rel`). First: `git checkout -B wt-hn1 <HEAD>` (given at launch). You are the only writer there. `tools/junction/jt` builds openttd_test before running.
- Why: the repairs of DESIGN-N3 §11 (ramp structure RS1, terraform TF1; D196) start from a Height failure. Today it carries only the Y class (`cls`). The repairs need to know which part of that class had no x (a rigid group, a free diagonal, or a Y-free column) and which tiles fail there.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); docs/junction-tool/TESTS.md; DESIGN-N3.md §10 and §11.
- src/junction_nodes.h :140-170 (JunctionNodeFailure, JunctionNodeSolve).
- src/junction_nodes.cpp :1030-1220: `snap`, `join_meets`, `prio_ok`, `diagonal_ok`, `column_ok`, `x_values`, `y_fits`, step 2b.
- src/tests/junction_nodes.cpp: "a house on a vertical leg fails on its class" (~:745) and "a free diagonal over a ramp row is a Height failure" (~:1183), and `FailureText`.

## Design (settled; implement exactly)
1. In junction_nodes.h, above JunctionNodeFailure:
   ```cpp
   /** Where a Height failure is (DESIGN-N3 §11): the first part of the Y class that had no x, at the first row the Y snap tried. */
   struct JunctionNodeHeightFault {
   	enum class Part : uint8_t {
   		Group, ///< A rigid group: the pieces of one X class on this Y class, with their joins and prio rows.
   		Diagonal, ///< A free diagonal whose two Y classes are fixed.
   		Column, ///< A Y-free link whose two Y classes are fixed.
   	};
   	Part part;
   	uint32_t index; ///< Group: the X class; Diagonal: index into tpl.diagonals; Column: index into tpl.links.
   	int row; ///< The Y class's value that was tried (layout frame of the template).
   	int x; ///< The X value at which the tiles were collected: the part's X class's flat value clamped to the snapshot range (x_values).
   	std::vector<TileIndexDiffC> tiles; ///< Working tiles failing at (row, x): a tile that does not fit its tracks, both tiles of a join that does not meet; in check order, no duplicates. Empty when the snapshot range is empty.
   };
   ```
   JunctionNodeFailure gains `std::optional<JunctionNodeHeightFault> height; ///< Height: where it failed.`
2. **The first row tried.** In step 2b, the row is the first `t` at which `snap` called `accept` (i.e. `RestrictRange(c, t, t)` succeeded). Record it without changing `snap`'s choice: the Y accept lambda passed to `snap` stores the first `positions.Lo(c)` it sees into a local `std::optional<int> first_row` (reset before each class).
3. **The part.** Order of parts is y_fits' order: groups (ascending X class, as the std::set iterates), then diagonals (tpl.diagonals order), then columns (tpl.links order). When `snap` fails: `RestrictRange(c, *first_row, *first_row)` again (set `result.values[c]` as snap does), find the first part whose check has no x (the same loops as y_fits), collect its tiles at x (item 4), then Undo to the mark. If `first_row` is empty (no row passed RestrictRange) leave `height` empty.
4. **Collecting tiles.** `prio_ok`, `diagonal_ok` and `column_ok` gain a last parameter `std::vector<TileIndexDiffC> *bad = nullptr`. When null they behave exactly as now (early returns; no speed change). When set they do not return early: every failing check pushes its working tile(s) (a Fit or Straight failure: that tile; a join failure: both tiles; `diagonal_ok` with a raster error: nothing) and the result is the AND of all checks. For a group, collect inline the same way: pieces failing JunctionNodeTrackFits, both tiles of each join of the group not meeting (from and to tiles shifted as `join_meets` shifts them), then `prio_ok(prio, x, bad)` for its prio rows. Deduplicate keeping first occurrence.
5. Step 2b sets `result.failure = JunctionNodeFailure{Kind::Height, UINT32_MAX, 0, c}` with `height` filled as above. Nothing else changes: the X filter (step 3) does not collect, and Piece failures are untouched.
6. `FailureText` in the tests prints the part, index, row, x and the tiles when `height` is set.

## Tests (outermost first; record each red)
1. Extend "a free diagonal over a ramp row is a Height failure": `REQUIRE(solve.failure->height)`; `CHECK(part == Diagonal)`; `CHECK(tpl.points[tpl.links[tpl.diagonals[index].link].from].ycls == cls or the .to point's ycls == cls)`; `REQUIRE(!tiles.empty())`; `CHECK` every tile's y is in [ridge, ridge + 3] (the ridge rows) and at least one tile's y is `ridge` or `ridge + 3` (a ramp row). Red: does not compile, then (member added, not filled) `REQUIRE(height)` fails.
2. Extend "a house on a vertical leg fails on its class": `REQUIRE(height)`; `CHECK(part == Column)`; `CHECK(index == leg)`; `CHECK` the blocked working tile is in `tiles`. Red: `REQUIRE(height)` fails.
3. New `[nodes]` "a Height failure's tiles fail at its row and x": the ridge terrain of test 1. Restate each reported tile's failure independently in the test: for the Diagonal part, rebuild the raster as the solve does is NOT required; instead CHECK that for every tile t in tiles, `facts.Fits(t.x, t.y, TrackBits{Track::X})` or `facts.Fits(t.x, t.y, TrackBits{Track::Y})` is false OR t's y is a ramp row (z_min != z_max). (A cheap consistency check; say in a comment that the solve's own collector is the authority.) Red: fails with `height` empty.
4. `[nodes]`, `[nodespeed]`, `[tmapnear]`, `[facts]`, `[nodeoracle]` stay green; no existing expectation edited (if one flips, STOP).

## Do not
- Do not change `snap`'s choice, y_fits' result, the X filter, the search, the oracle, the facts, or any failure kind. No repairs.

## Stop conditions
- The first-row rerun in item 3 finds no failing part (y_fits disagrees with itself): report the class, row and template sizes.
- `[nodes]` Release over 1 s, `[nodespeed]` slower than its TESTS.md budget.

## Gates
`tools/junction/jt fast build "[nodes]" 5`, `tools/junction/jt fast build-rel "[nodes]" 1`, `tools/junction/jt fast build-rel "[nodespeed]" 5`, `tools/junction/jt fast build-rel "[tmapnear]" 5`, `tools/junction/jt gate build-rel`. Update TESTS.md times for tags you ran. Commit: `Feature: Junction node Height failure names its part and tiles`, body with the ridge fault (part, index, row, x, tiles) and the house fault, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
References read; commit; red evidence per test; types and members added/removed; the two faults as printed; suites with times; stops; `git status --porcelain` and `git log -1`. When you have reported, stop.
