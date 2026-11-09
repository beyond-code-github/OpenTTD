# O4x: why the line router's uninformed search returns a costlier route (investigation, no fix)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/o4x`, branch `wt-o4x`, from `wip-o4h2` (head in your launch prompt). You are the only writer there; never touch sibling worktrees or the main worktree. Never use `git stash`.
- Build: configure once, `cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18 -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache > cfg.log 2>&1`, then `cmake --build build-rel --target openttd_test -j 8 > build.log 2>&1`.
- Run single tests alone (`build-rel/openttd_test "<name>" > x.log 2>&1`); the machine is unstable under all-core load.

## The symptom
On `wip-o4h2` (Manhattan heuristic), `[slow] "FindJunctionLineRoute: the route is as cheap as an uninformed search"` (src/tests/junction_line_router.cpp ~437) fails: for some L_R / LL_RR goals the informed search (heuristic on) returns a CHEAPER route than the uninformed one (`use_heuristic = false`, h = 0), e.g. 251 vs 259. An exact Dijkstra can never return a costlier route than any legal route, so the search's pruning is not exact: the `best` map keyed by `Key(node)` (junction_line_router.cpp, `push`) discards a node whose key matches an earlier, cheaper-or-equal node although the discarded one could still lead to a cheaper legal completion (state that affects future legality or cost but is not in the key: for example the path's own used tracks `path_used`, S-bend/D110 closing state, curve spacing history, bridge history).

## What to find (in this order; stop when 1-4 are answered)
1. One failing case: notation, goal tile, trackdir, both costs and both routes' move strings (`route->moves`). Print them from a temporary test or temporary INFO; do not commit those prints.
2. Confirm the informed route is legal: replay its moves through the same legality the router uses (rasterize with `RasterizeJunctionLine` and check it with the existing `CheckRoute` helper in the test file, or feed it as a `prefix` to a request with that goal and confirm the router accepts it). Report how you checked.
3. Find where the uninformed search lost it: the first node on the informed route's path whose push was rejected by `best` (same key, `best[key] <= g`), the node that held that key, and the field-by-field difference between their full states (the key fields, `path_used` contents, and every other member of `LineNode` and of the tree entry that legality or cost reads later). Temporary instrumentation only.
4. Name the missing state: which member, if added to the key, would have kept the cheaper node; and which later check reads it (file:line).
5. Only if 1-4 are clear: measure, with temporary code, how many more nodes the uninformed and informed searches expand on the benchmark cases (`FindJunctionLineRoute: route cost benchmark`) if that member is added to the key. Report the numbers; do not keep the change.

## Do not
- Commit any change to non-test files. Commit nothing unless it is a findings file (below).
- Change the heuristic, costs, or tests' expectations.

## Report
Write the findings to `docs/junction-tool/FINDINGS-O4x.md` (the case, the legality check, the pruned node and the state difference with file:line, the missing member, step 5's numbers if done) and commit only that file: `Doc: Junction line router pruning findings (O4x)`, body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. In your report: a short summary of the findings, `git status --porcelain` and `git log -1`. Revert all temporary instrumentation before committing. When you have reported, stop.
