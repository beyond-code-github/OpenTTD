# P2: the line router's per-expansion cost (FINDINGS-P1 F2, F3, F4), and the T map's line-router path behind a flag

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/p2`, branch `wt-p2`, head given at launch (feature/junction-tool). Create `build` (Debug) configured like `/home/pete/OpenTTD/build` and `build-rel` (Release) like `/home/pete/OpenTTD/build-rel` (Ninja, clang-18). Build `openttd_test` only.
- Findings: `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/p1/FINDINGS-P1.md` (read F2, F3, F4 and section 3). Line router per expansion: 2.5-3.9 us Release against 0.3-0.5 us for the old router.

## Test speed
Develop against `[junctionmap]` and `[linerouter]` (report their times at the start; each must stay under its starting time + 1 s in Debug). Wider gate once at the end.

## Read first (once each)
`CODINGSTYLE.md`; `src/junction_line_router.cpp` lines 30-70 (`LineNode`, `LineTreeEntry`), 320-345 (`Key`), 560-700 (roots, `push`, the loop, the path walk), 850-870 (the bridge successors); `ExtendJunctionLineBridge` in `src/junction_line.cpp` (where it places each track's far head).

## Commit 1: the T map's line-router path, default off
Apply `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/p1/part2_tmap_rebased.patch` (`git apply`; on failure stop). Then change its default to `bool line_router = false` (the old router stays the default until the line router is fast enough; the generation target is < 100 ms Release). In its test "the line-router T matches or beats the old-router T", replace `CHECK(line.micros_route < old.micros_route)` with printing both route times on a `BENCH` line (no assertion on time); the compactness and clean checks stay. `[junctionmap]` green. Commit: `Change: Junction T map edges can be routed by the line router`.

## Commit 2: per-expansion cost
Golden first (red-equivalent for a refactor): before changing the router, add a test `"line router results are unchanged by the speed work"` tagged `[linerouter]` that, for both T-map specs with `line_router = true`, records each line-router request's route moves string, cost and expansions (add `std::string line_router_trace; ///< Test aid: one line per line-router request: moves, cost, expansions.` to `JunctionTMapResult`, appended inside the `route_line` lambda) and compares them with the expected strings you paste into the test from the run before your change. It passes before the change; it must pass unchanged after.
Then:
1. F2: add `int root = -1; ///< Tree index of this entry's root.` to `LineTreeEntry`, set at push (a root is its own root; a child takes its parent's), and replace the two walks to the root (`IsGoal` branch and the path walk) by it.
2. F4: add `std::string key; ///< Key(node), computed once at push.` to `LineTreeEntry`; `push` computes the key once and stores it; the pop-side check uses `best.find(tree[index].key)` (never `operator[]`).
3. F3: in the bridge successor loop, before `ExtendJunctionLineBridge`, compute track 0's far head tile from `node.last[0]` and n exactly as `ExtendJunctionLineBridge` places it; if that tile is outside `[request.min, request.max]`, `break` (every larger n is further along the same heading). State this reasoning in a comment.
No other change. Commit: `Change: Junction line router expands faster`.

## Measure (report)
Release: per expansion, before and after (as FINDINGS-P1 measured: `micros_route` / total expansions from the trace, medians of 20 runs of `ConstructJunctionTMap` with `line_router = true`), both specs. Also the full T-map time with each router after the change.

## Do not
No other findings (no allocator tuning, no memoisation, no packed keys, no expansion caps, no small vectors). No change to search order, costs, heuristics, legality or results. No change to `junction_router.*`, the generator's decisions, `junction_organic.*`, savegame, network or command code. No `std::to_string`/`std::printf` in tests; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
- The golden test changes after any of the three changes (results must be identical, expansions included).
- The patch does not apply.
- A tag's time grows by more than 1 s.

## Gates
0 warnings; `[junctionmap]`, `[linerouter]`, `[linebuild]` green; once at the end `"[junction]~[slow]~[mapexport]"` green (report time). Two commits on `wt-p2`, each ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commits; golden test evidence (passes before and after); members added; per-expansion times before and after; T-map times both routers; suite results with times; stops; `git status --porcelain`, `git log -2`.

When you have reported, stop.
