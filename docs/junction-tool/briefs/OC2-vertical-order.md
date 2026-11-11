# OC2: vertical order chain in the Y phase (DESIGN-N3 §9; D193)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
Worktree `/home/pete/OpenTTD/.claude/oc2`, branch `wt-oc2`, head given at launch. Create `build` (Debug) and `build-rel` (Release) (clang-18, ccache); build `openttd_test` only. Tests only through `tools/junction/jt`: `'[nodes]'` (budget 1 s Release, 5 s Debug); `jt gate build-rel` once before the commit. Output in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/oc2/`.

## Read first (once each, offset/limit)
`CODINGSTYLE.md`; `docs/junction-tool/DESIGN-N3.md` §5-§9; `src/junction_nodes.h`; `src/junction_nodes.cpp` (BuildJunctionNodeTemplate and its builder, AddJunctionNodeOccupancy: reuse its owner-set and element ideas); `src/tests/junction_nodes.cpp` (the hidden RU1 test "a short waiting room is lengthened" and the occupancy tests).

## Change
In BuildJunctionNodeTemplate, after the existing rules, add the vertical chain of DESIGN-N3 §9 as static diffs into tpl.diffs (Y variables only), in a new NodeBuilder member `void AddVerticalOrder();`. Owner sets exactly as AddJunctionNodeOccupancy defines them (a free link's tiles own that link; a piece owns the free links ending on its flat tile). Flat overlap in a column (flat hi_a >= flat lo_b, owners disjoint) -> Fail("column <x>: elements stacked out of order at rows <a>,<b>").

## Tests, red first (`[junction][nodes]`). Red: AddVerticalOrder empty; record.
1. (outer) Un-hide RU1's "a short waiting room is lengthened" (remove `[.]` and its comment): it must pass (layout found, no WaitingRoom violation at tl 8). Red: Occupancy at row 12.
2. "the vertical chain keeps a stacked pair apart" (inner): FlatT; positions; AddJunctionNodeConstraints; pick the W line's lane A row and A's row (rows 3 and 1 in the flat, lane A above... use the pair the chain constrains in some column between A (row 1) and the gap row/lanes: choose any consecutive pair the chain emits with flat gap >= 2 and WARN it); add the opposite (lo_b - hi_a <= 0) via AddDifference: CHECK false.
3. All existing [nodes] tests pass (100-196 km/h, searches, occupancy, RU1 tests).

## Do not
No change to the X-phase occupancy chain, the solve order, junction_positions, the checker, the generator. No `std::to_string`/`std::printf`; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
A builder Fail on the flat T (report verbatim); an existing test failing (report verbatim); test 1 still failing (report the failure and the solve's values that moved); OVER BUDGET; anything not covered.

## Gates
0 warnings; `jt fast build '[nodes]' 5`, `jt fast build-rel '[nodes]' 1`, `jt gate build-rel`. One commit on `wt-oc2`: `Feature: Junction node vertical order chain in the Y phase (DESIGN-N3 §9)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence; members added; number of vertical diffs on the flat T; test 1's layout size vs the flat; `[nodes]` times; gate; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
