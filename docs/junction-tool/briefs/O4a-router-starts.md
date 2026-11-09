# O4a: the line router takes a set of start pieces (DESIGN-O4)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/o4a`, branch `wt-o4a`, head given in your launch prompt. You are the only writer there; never touch sibling worktrees or the main worktree. Never use `git stash` (the stash is shared): to compare against the old code, use `git show HEAD:<file>` into a scratch copy or a temporary commit you drop.
- Build: configure once as in `briefs/O1-router-heights.md`; `cmake --build build-rel --target openttd_test -j 8 > build.log 2>&1`.
- Tests: `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/ptest.sh build-rel/openttd_test "<filter>" 8`; `"[linerouter]~[slow]"` to iterate; `"[junction]~[slow]"` then `"~[.]"` at the gate. A lone segfault/SIGILL under load is the machine: rerun alone and report.
- Design: `/home/pete/OpenTTD/docs/junction-tool/DESIGN-O4.md` ("Movements": starts = every plain piece of the source track inside its split window).

## Why
In the organic junction each movement leaves its source track anywhere inside a rule window, so the router must start from any of several pieces and pick the best.

## Read first
- `CODINGSTYLE.md`; DESIGN-O4.md "Movements".
- `src/junction_line_router.{h,cpp}` (whole) and the helpers at the top of `src/tests/junction_line_router.cpp`.

## Tests (red first)
1. "FindJunctionLineRoute: picks the better of several starts": L_R, two start pieces (a far one and a near one, same heading), one goal: the route starts at the near one (result route's `start`), cost equals the single-start route from it. Red: the member doesn't exist (compile), then with only the first start used, the far start is taken.
2. "FindJunctionLineRoute: one start in the set equals the single start": the "straight when clear" request with `starts = {{start, start_trackdir}}` gives the same route and cost as today.
3. "FindJunctionLineRoute: starts with a prefix is an error": `starts` with two entries and a non-empty `prefix` returns an error (a frozen prefix belongs to one start).
4. The `[slow]` informed = uninformed case extended with test 1's request.

## Changes
1. `junction_line_router.h`, request: `std::vector<JunctionRouteEnd> starts; ///< When not empty, the route may start at any of these (track 0's first piece and trackdir) and `start`/`start_trackdir` are ignored (DESIGN-O4).`
2. `junction_line_router.cpp`: the root becomes one root per start (each built and checked as today's root, then pushed); unusable starts are skipped; with no usable start, "no route". `starts` with more than one entry and a non-empty `prefix`: error `"a prefix needs a single start"`. The result's route carries the start it used. The path state of each entry must trace back to its own root (each root's first pieces occupied for its own subtree only).

## Do not
- Change any move, cost, height, bridge, waypoint or heuristic rule.

## Gates
1. Build: no errors, no new warnings. 2. `"[junction]~[slow]"`, then `"~[.]"`. 3. Commit: `Feature: Junction line router may start from any of several pieces`, body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
References read; commit; red evidence per test; names added; suite results; stops; `git status --porcelain` and `git log -1`. When you have reported, stop.
