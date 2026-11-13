# G3.3 P1: are bend rasters affine in their length? (DESIGN-G3 §G3.3a, D236 premise)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
One characterisation test that says whether every bend's raster quantities are affine in its length. No production code changes.

## State
Worktree `/home/pete/OpenTTD/.claude/nodes2`. First: `git -C /home/pete/OpenTTD/.claude/nodes2 checkout -B wt-p1 <HEAD given at launch>`. Builds `build` (Debug) and `build-rel` exist. Output files in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/p1/`. Another agent works in `.claude/nodes`: never touch it.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`;
- `docs/junction-tool/DESIGN-G3-vectors.md` §G3.3a (premise P1);
- `src/junction/junction_track_geometry.h` (`JunctionBendMoves`, `JUNCTION_AXIS_TRACKDIR`);
- `src/junction/junction_route.h` 15-55;
- `src/junction/placement/junction_placement.cpp` 500-520 (how halves come from m: S-bend and Fan `2m`, Corner `2k + 1`) and 800-830 (`Pieces`: how `before_dx/dy`, `close_dx/dy`, `close_units` and the curve units are read from a raster);
- `src/tests/junction/junction_track_geometry.cpp` (its style; the new test goes there).

## Test (`src/tests/junction/junction_track_geometry.cpp`, tag `[junction]`)
"bend rasters are affine in their length". For corner in {false, true}, heading 0..3, hand in {-1, +1}, L = 1..64, halves = corner ? 2L + 1 : 2L:
- raster = `RasterizeJunctionRoute({{0, 0}, JUNCTION_AXIS_TRACKDIR[heading], JunctionBendMoves(hand, halves, corner)})`; REQUIRE empty error and `pieces.size() == halves + 2`.
- The quantities, read exactly as placement reads them at ~800-830: `before_dx`, `before_dy`, `close_dx`, `close_dy`, `close_units`, the number of curves, and each curve's units.
- For each quantity q: CHECK `q(L) == q(1) + (L - 1) * (q(2) - q(1))` for L >= 3, and for each curve, in index order, the same for its units (the number of curves must be constant; CHECK that first).
- INFO corner, heading, hand, L and the quantity name.
This is characterisation: no red step. If any CHECK fails, do not change the test to pass. Record each failing (corner, heading, hand, quantity) with q(1), q(2), q(3) and q(4), at most 30 lines, then commit the test as it is but tagged `[.p1]` (hidden), so the suites stay green, and report.

## Do not
- Change any file outside `src/tests/junction/junction_track_geometry.cpp`.
- Use `pkill`, a broad `kill`, or a bare `git stash`.

## Gates
- 0 new warnings;
- `tools/junction/jt fast build '[junction]'` with the budget TESTS.md names for it (if TESTS.md has no such row, run the single test by name and report its time);
- `tools/junction/jt gate build-rel`.
One commit on wt-p1: `Add: Junction test that bend rasters are affine in their length`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; the commit; affine yes or no; the failure list if any; gate counts and times; stops; `git status --porcelain` and `git log -1`. When you have reported, stop.
