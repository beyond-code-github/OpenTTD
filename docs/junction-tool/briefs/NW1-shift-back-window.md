# NW1: node windows, first case: N->B's failure moves the shift-back west (D178, CS2)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Why
Diagnosis `docs/junction-tool/DIAG-ph1-limit.md` (read it): in "a height change inside the junction's W end is not built yet", N->B's merge goals are lane B's b-row pieces 17..20 (`x_sb + 1 .. x_nw`); the incline column (x 19) refuses the merges at 18 and 19, 20 is not a real merge (its merge tile would be past `x_nw`), and 17 is refused by CS2 because lane B's shift-back S-bend at `x_sb = 16` follows it directly. `x_nw` is already at its PrioCoverage maximum, but the shift-back may legally lie further west: the skeleton's ShiftBack rule is `x_nw - x_sb >= k_sb` (`k_sb` pinned by the caller, `JUNCTION_SHIFT_BACK_TILES`). Node windows, first case: when N->B fails, its goal window is widened by re-solving the skeleton with the shift-back one tile further west, and the dependent edges are routed again, up to a bound from the rule that refused it.

## State
- Worktree `/home/pete/OpenTTD/.claude/nw1`, branch `wt-nw1`, head given at launch (feature/junction-tool). Create `build` (Debug) and `build-rel` (Release) like the main ones (clang-18 with ccache); build `openttd_test` only.
- Tests only through `tools/junction/jt` with bracketed tags (TESTS.md): `'[projectionedges]'`, `'[projection]'`, `'[tmapnear]'` in Release, `'[junctionmap]'` and `'[junctionmapedges]'` in Debug, final gate. The `[slow]` test named below is run once by exact name in Release, the only by-name run allowed. OVER BUDGET is a stop.

## Read first (once each)
`CODINGSTYLE.md`; the diagnosis; `JunctionSkeletonInputs` (`k_sb`) in `src/junction_skeleton.h` and the ShiftBack constraints in `src/junction_skeleton.cpp`; `ConstructJunctionTMap`'s line-router path in `src/junction_generator.cpp` from the skeleton inputs (`in.k_sb = JUNCTION_SHIFT_BACK_TILES`, the stagger loop) through `BuildParallelogramLanes`, B's lane edges, B->S and N->B; the test named below in `src/tests/junction_map.cpp`.

## Tests, red first
1. "a height change inside the junction's W end is not built yet" becomes "a height change inside the junction's W end moves the shift-back": expected now a layout at the flat height, `clean`, and a log line `N->B failed; shift-back moved west by d` for each d tried before it built. Drop `[slow]` and its comment if its Release time is under 0.5 s (else keep `[slow]`, report the time). Red: today no layout.
2. Every existing test keeps its outcome and layout; the flat identity and the golden trace are unchanged (on flat ground N->B routes at d = 0).

## Change (`src/junction_generator.cpp`, `ConstructJunctionTMap`, line-router path only)
1. Wrap the part from the skeleton inputs (the stagger loop that chooses `pos`) through N->B in a loop over `d = 0 .. spec.tl` with `in.k_sb = JUNCTION_SHIFT_BACK_TILES + d`. Bound comment: CS2 refuses a merge whose continuation turns within the S-bend window (`s_bend_window = 2 * tl` units); a straight tile is 2 units, so `tl` extra tiles of straight before the shift-back clear it; more cannot help N->B's merge.
2. Each iteration starts from the layout as it was before the skeleton (A, the port edges, A->S, N->A routed): rebuild the lanes from the re-solved skeleton, route B's lane edges, B->S and N->B as today. If N->B fails and `d < spec.tl`: `say(fmt::format("N->B failed; shift-back moved west by {}", d + 1))` and continue with d + 1. Any other failure, an infeasible skeleton for this d, or N->B failing at `d == spec.tl`: behave exactly as today for that failure (say it, next height, with the existing growth handling).
3. Keep the routed-raster bookkeeping (`routed_rasters`, CS1/CS2) consistent: rasters routed in a discarded iteration must not be found by `approach` or `continuation` in the next (remove them or rebuild the list from the kept edges; say which).
4. Nothing else: no change to the solver, the other skeleton constants, the router, the old-router path.

## Do not
No change to `src/junction_skeleton.*`, the router, the checker, the old-router path, decisions, savegame, network or command code. No `std::to_string`/`std::printf` in tests; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
- Test 1 not clean after the loop: report the log and the violations.
- Any existing test, the flat identity or the golden trace changes.
- OVER BUDGET; anything not covered.

## Gates
0 warnings in both builds; the jt runs above pass. One commit on `wt-nw1`: `Change: Junction N->B's failure moves the shift-back west`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence; test 1: d that built, height, Release time; how discarded rasters are kept out of routed_rasters; jt lines; stops; `git status --porcelain`, `git log -1`.

When you have reported, stop.
