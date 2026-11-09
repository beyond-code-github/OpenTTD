# O4p: line router per-expansion speed and the D161 test oracle

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/o4p`, branch `wt-o4p`, head in your launch prompt: the feature branch plus the WIP commit "Junction line router heuristic, Manhattan distance and line-level floor". You are the only writer there; never touch sibling worktrees or the main worktree. Never use `git stash`.
- Build: configure once, `cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18 -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache > cfg.log 2>&1`, then `cmake --build build-rel --target openttd_test -j 8 > build.log 2>&1`.
- Tests: `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/ptest.sh build-rel/openttd_test "<filter>" 8`. The benchmark runs alone, never beside a build or tests: `build-rel/openttd_test "FindJunctionLineRoute: route cost benchmark" > bench.log 2>&1`.
- Decisions: DECISIONS rows D161 (read first) and the findings `docs/junction-tool/FINDINGS-O4x.md`.

## Why
One route must cost well under 1 ms for the organic junction search (six movements per round, several rounds, 100 ms per generation). After the heuristic work single-track turns cost 3-18 ms. The O4h agent named per-expansion hot spots in `src/junction_line_router.cpp`: `Key()` formats a string with `fmt::format` per push and per pop; every successor copies the whole `path_used` map (`used = path_used`), including every bridge and tunnel length tried. The WIP commit also fails the `[slow]` equality oracle for the reason D161 rules on.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`; DECISIONS D161; FINDINGS-O4x.md.
- `src/junction_line_router.cpp` whole (it is the file you change), `src/junction_line_router.h`.
- The `[slow]` "as cheap as an uninformed search" tests and the benchmark in `src/tests/junction_line_router.cpp`.

## Change 0 first (D162): the heuristic for lines of more than one track
A first run of this brief stopped at change 1: with several starts, LL_RR, goal {20,20} X_NE, the informed search returned 80 where the uninformed found 74 (dy 3) and 80 vs 78 (dy 4). Cause (D162): the WIP heuristic charges every track the Manhattan distance of track 0, but a track on the inside of a curve travels fewer tiles than track 0, and terrain already paid by a track ahead of track 0 is not paid again, so the bound can overestimate for lines of more than one track. Make `Heuristic` exactly:
- `request.line.tracks == 1`: as today (`step_floor * distance + turn_cost * turns`, with `step_floor` = min(rail_piece + terrain floor, bridge floor, tunnel floor) for one track).
- `request.line.tracks > 1`: `track_floor * distance + turn_cost * turns`, where `track_floor` = min(rail_piece, bridge floor, tunnel floor) (no terrain, one track only; the bridge and tunnel floors are `price(n) / (n + 2)` as today).
Compute both floors once in the constructor (two members: `step_floor` keeps its meaning for one track, a new `int64_t track_floor` for lines of more than one track); update the comments on `step_floor`, `track_floor` and `Heuristic` to say why (D162). The previous run's change-1 test diff is at `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/o4p-change1.diff` (apply it with `git apply` for change 1).

## Changes, in this order (build and run `"[linerouter]~[slow]"` after each; record the benchmark once before change 2 and once after change 3)
1. Tests (D161), after change 0: in every `[slow]` "... as cheap as an uninformed search" test, replace `CHECK(informed.cost == uninformed.cost)` with `CHECK(informed.cost <= uninformed.cost)` and add the comment `/* D161: pruning is approximate for lines whose tracks can collide; an overestimating heuristic shows as a costlier informed route. */` once per test. Everything else in those tests unchanged. Run `"[linerouter][slow]"`: it must pass. If a case has informed > uninformed, stop and report it.
2. `Key()`: build the same fields into the string as raw bytes instead of formatted text (`key.reserve` once, then append each field's bytes, for example with a small `static void AppendBytes(std::string &key, const T &value)` that appends `sizeof(T)` bytes of a trivially copyable value). The key must stay equal exactly when today's key is equal (same fields, same order, fixed widths so fields can't run together). Expansion counts on the benchmark must not change: compare `expansions` per case before and after (print them in the benchmark line: add ` expansions=<n>` to the BENCH output).
3. Successor occupancy: keep `path_used` built once per expansion as today, but successors no longer copy it. Each successor keeps a small local overlay (`std::vector<std::pair<uint64_t, TrackBits>>`) of the tracks its own added pieces set; `Occupy` becomes a function of (`const` base map, overlay, piece) that reads base | overlay bits for the tile, refuses as today, and records the new bit in the overlay. Every reader of `used` in the successor code (the `Occupy` calls and the `used.contains` checks, and the terrain-cost "tile already used" check) reads base | overlay with the same meaning as today. Expansion counts and costs on the benchmark must not change.

## Do not
- Change the key's fields, the heuristic, costs, moves, legality, `max_expansions`, the open list or the tree.
- Add a reachability pre-check or any other optimisation not listed (report ideas as findings).

## Stop conditions
- After change 0, any `[slow]` case has informed > uninformed (report the case).
- Expansions or costs on any benchmark case change after change 2 or 3.
- Any existing test fails (beyond a lone hardware flake: rerun alone and report).

## Gates
1. Build: no errors, no new warnings.
2. `ptest.sh build-rel/openttd_test "[linerouter]~[slow]" 8`, `"[linerouter][slow]"` (6 jobs), `"[junction]~[slow]"`.
3. Benchmark alone three times after change 3; report BENCH lines of the before run and all three after runs.
4. Commit: `Change: Junction line router admissible multi-track heuristic, keys and occupancy without formatting or copies`, body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. One commit for changes 0-3.

## Report
References read; commit; benchmark lines (before change 2, three after change 3) with expansions; suite results; findings (other hot spots you saw, with the line); stops; `git status --porcelain` and `git log -1`. When you have reported, stop.
