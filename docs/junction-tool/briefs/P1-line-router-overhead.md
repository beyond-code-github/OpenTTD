# P1: where the line router's per-request time goes (recon, no commit)

You execute this brief and nothing else. This is measurement only: you change no tracked source file and make no commit. If something is not covered, or you would have to choose between approaches, stop and report the question. Keep your context small: send command output to files and report only what failed.

## Background
G2a measured `FindJunctionLineRoute` 7-8x slower than the old router (`junction_router.cpp`) per successful request on the T map's four edges in Debug, with similar layouts: 58-817 expansions costing about 8 ms, which points at per-request or per-expansion overhead, not search size. The T map's switch to the line router waits on this.

## State
- Worktree `/home/pete/OpenTTD/.claude/p1`, detached at the head given at launch (feature/junction-tool). Create `build-rel` configured like `/home/pete/OpenTTD/build-rel` (Release; read its CMakeCache for generator and flags, add `-g` via `CMAKE_CXX_FLAGS_RELEASE` only if it lacks debug info, and say so). Build `openttd_test` only.
- Apply `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/g2/part2_tmap.patch` (`git apply`; if it does not apply, stop and report). It routes the T map's edges with the line router behind `line_router = true`.

## Read first (once each)
`src/junction_line_router.h` (whole); in `src/junction_line_router.cpp` the request setup, the open set, the closed/visited key and the successor generation; `src/tests/junction_map.cpp`.

## Measure
1. Release, the T map with each router (the patch's test that compares them, or `ConstructJunctionTMap` called from a temporary test you add to the untracked worktree only): per edge, expansions and microseconds for both routers. Run each 20 times and give medians.
2. `perf record -g` (if `perf` is missing, use `valgrind --tool=callgrind` on one run; if neither exists, stop and report) on the line-router T map run in a loop of 200: the top 15 functions by self time and the top call paths, with percentages.
3. Split the line router's time into: setup before the first expansion (allocations, copying `existing`/`blocked`/terrain, building indexes), per-expansion cost, and result building. Give microseconds per request for setup and per expansion for the loop.

## Do not
No change to tracked files outside the untracked test; no fixes, even obvious ones (list them as findings with the evidence). No `pkill`, broad `kill`, bare `git stash`.

## Output
Write `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/p1/FINDINGS-P1.md`: the tables above, the profile excerpt, and up to five findings, each with: what costs the time (function, line), the measured share, and the smallest change that would remove it. Report: the findings file's path and a five-line summary.

When you have reported, stop.
