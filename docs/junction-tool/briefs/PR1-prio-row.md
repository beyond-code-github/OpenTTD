# PR1: reserve the prio row in the occupancy chain (DESIGN-N3 §8; D193)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
Worktree `/home/pete/OpenTTD/.claude/pr1`, branch `wt-pr1`, head given at launch. Create `build` (Debug) and `build-rel` (Release) (clang-18, ccache); build `openttd_test` only. Tests only through `tools/junction/jt`: `'[nodes]'` (budget 1 s Release, 5 s Debug); `jt gate build-rel` once before the commit. Output in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/pr1/`.

## Read first (once each, offset/limit)
`CODINGSTYLE.md`; `docs/junction-tool/DESIGN-N3.md` §7-§8; `docs/junction-tool/RECON-prio-footprint.md`; `src/junction_nodes.h`; `src/junction_nodes.cpp` (AddPathDiffs' prio detection, AddJunctionNodeOccupancy); `src/tests/junction_nodes.cpp` (helpers, the search tests, the occupancy tests).

## Changes
Header:
```cpp
/** A prio merge's reserved row (DESIGN-N3 §8). */
struct JunctionNodePrioRow {
	uint32_t merge; ///< The merge point on the main-line path.
	int8_t side; ///< +1 or -1: the row of the prio relative to the merge's row (opposite the joiner).
	int8_t dir; ///< The main line's x direction at the merge.
};
struct JunctionNodeTemplate { ...; std::vector<JunctionNodePrioRow> prio_rows; };
```
`.cpp`:
- AddPathDiffs: for each prio merge it detects, push a JunctionNodePrioRow once per merge point: dir = the ML path's dir; side = -(row of the joiner's piece just before the merge tile - row of the merge); the joiner is the other path whose switch event shares the merge tile (as the prio detection finds it). Joiner piece on the merge's row (offset 0): no prio row (PlacePrio lays none there) and no error.
- AddJunctionNodeOccupancy: one element per prio row: current row = the merge's current row + side; lo/hi = the merge's x (variable cls, offset) and that x - (prio_tiles + 1) * dir, ordered by x; bits Track::X; empty owner set; flat row = flat row of the merge + side; flat lo/hi the same at flat values. Nothing else changes in the chain.

## Tests, red first (`[junction][nodes]`). Red: prio_rows not filled; record.
1. "the flat T reserves its two prio rows" (inner): FlatT at the test spec: exactly 2 prio rows; A's: side -1 (row 0), x range [23, 38] at flat values; the W line's: side -1 (row 3), x range [6, 21]. (If the merge tiles or sides differ from these, report the actual values and stop.)
2. "a forced line in the prio row is refused" (inner): FlatT; positions; AddJunctionNodeConstraints; force N->E's X run (row 2, the run between its diagonal and the prio merge S-bend) one row up into A's prio row: the run's Y variable minus A's Y variable equal to -1 (two diffs; REQUIRE added; if they are static-infeasible, report and pick no other target: stop); CHECK JunctionNodeSnap returns empty. If the Y force is infeasible statically, replace this test by a WARN and report.
3. All existing [nodes] tests pass, including the house and hub-tile searches (REQUIRE layout). If a search now finds no layout within its state limit, that is a measurement: report its full log verbatim and stop uncommitted.

## Do not
No change outside src/junction_nodes.* and src/tests/junction_nodes.cpp. No join-tile-per-block rule, no PrioReach, no bending prio. No `std::to_string`/`std::printf`; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
As in the tests; any existing test failing (report verbatim); OVER BUDGET; anything not covered.

## Gates
0 warnings; `jt fast build '[nodes]' 5`, `jt fast build-rel '[nodes]' 1`, `jt gate build-rel`. One commit on `wt-pr1`: `Feature: Junction node prio rows reserved in the occupancy chain (DESIGN-N3 §8)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence; members added; test 1's values; test 2's result; the house/hub searches' states and times; `[nodes]` times; gate; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
