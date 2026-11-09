# S2e: parallelogram lanes from solver positions (D118 stagger), wired into the candidate builder

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/s2e`, branch `wt-s2e`, head given in your launch prompt. You are the only writer there. Other agents work in sibling worktrees under `.claude/`; never touch them or the main worktree `/home/pete/OpenTTD`.
- Build: configure once, `cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18 -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache > cfg.log 2>&1`, then `cmake --build build-rel --target openttd_test -j 8 > build.log 2>&1` (ccache is shared, so it is fast after the first build). Debug only if the gate asks.
- Tests: `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/ptest.sh build-rel/openttd_test "<filter>" 8` (8 jobs: other agents share the machine). Iterate with your own tag; full `"~[.]"` only at the gate.
- Decisions: `/home/pete/OpenTTD/docs/junction-tool/DECISIONS.md` (read the rows named below; it is newer in the main worktree than in yours).
- Plan: `docs/junction-tool/PLAN-S2.md` §3, §4 (S2e; S2c is folded in here). Decisions D145 (the measured constants: read it first), D144, D118, D113, D110, D108, D105, D117/D117b. Geometry evidence: `docs/junction-tool/RECON-lane-geometry.md`.

## Why (settled)
The candidate builder searches lane programs with one shared head x for both lanes and a shared `gap`, so D118's stagger can't be built and the gap adds a tile at both ends. D145 measured the geometry: with one signal-legal piece per leg, lane B's heads sit one tile further +x than lane A's at both ends (x_ls − x_heA = 2, x_ls − x_heB = 1, x_hwA − x_lm = 2, x_hwB − x_lm = 3), x_sl − x_ls = 2, k_head = 1. This step makes the parallelogram style (lane_style 1) use the S2d solver (`src/junction_skeleton.{h,cpp}`) for its positions and build both lanes from them. Style 0 is unchanged.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`.
- `docs/junction-tool/DECISIONS.md` rows D145, D144, D118; `RECON-lane-geometry.md` §c, §e.
- `src/junction_skeleton.{h,cpp}`, `src/tests/junction_skeleton.cpp` (all).
- `src/junction_route.cpp` 1-140 (`RasterizeJunctionRoute`, the 'B' move), `src/tests/junction_route.cpp` (grep 'B').
- `src/junction_generator.cpp` 210-520 (`BuildJunctionCandidateWithGap`: pass 1, zone, lane programs, `route_movements`), 590-630 (`BuildJunctionCandidate`).
- `src/tests/junction_generator.cpp` 1-70, 225-300.

## Changes
1. **Solver inputs (S2d update)** in `junction_skeleton.{h,cpp}`:
   - `JunctionSkeletonInputs` gains `int stagger = 0; ///< Lane B's heads minus lane A's, at both ends (D118: 0 or 1).` The EqualLength constraints become x_heB − x_heA = stagger and x_hwB − x_hwA = stagger (two constraints each). Remove the rule that derived s from zmin/zmax, and its comment.
   - Defaults from D145: `k_split = 2`, `k_entry = {2, 1}`, `k_exit = {2, 3}`, `k_head = 1`. `k_w` and `k_sb` stay inputs (pinned below).
   - Update `src/tests/junction_skeleton.cpp` expectations to the new constraints (recompute by hand; show the arithmetic in the report). Keep the infeasible-span test.
2. **Bridge right after a bend** in `RasterizeJunctionRoute`: a 'B' move whose current piece is a half piece is allowed when continuing straight from that piece gives an axis trackdir (`JunctionNextTrackdir(cur, DirDiff::Same, next)` with `IsDiagonalTrackdir(next)`); the near head is then the next tile with trackdir `next`. Otherwise the existing error stays. Test in `src/tests/junction_route.cpp` "RasterizeJunctionRoute: a bridge may start right after a bend (D110)": `"RFLB2"` from an X_NE start gives the near head on the tile after the L half piece and the far head 3 tiles further. All existing route tests unchanged.
3. **Lane builder**, in `junction_generator.cpp` as a file-local function used by style 1:
   `static std::optional<std::pair<JunctionRaster, JunctionRaster>> BuildParallelogramLanes(const JunctionSkeletonW &pos, int width, int w_row, int b_row, std::string *error)`.
   - Lane A from `(width − 1, w_row)` heading X_NE: `F^k B<nA> F^g RFL`; lane B: `F^p RFL F^q B<nB> F^t LFR F^s`, where a "q = −1" form means `RFL` directly followed by `B<nB>` (change 2). n = x_he − x_hw − 1 per lane.
   - Find each count by rasterise-and-match in increasing order, exactly as today's code matches (same definitions): A: near head = (x_heA, w_row), far head = (x_hwA, w_row), last piece on `b_row` with x == x_lm. B: first non-axis piece x == x_ls, near head (x_heB, b_row), far head (x_hwB, b_row), last piece before LFR at (x_sb + 1, b_row), end at (0, w_row). Keep today's merge check (A's last piece lands where B has an axis piece). On failure set `*error` to the first unmet target and return nullopt.
4. **Wiring, style 1 only**, in `BuildJunctionCandidateWithGap`:
   - Pass 1 also records the outer-row crossings: for E->S and N->E, the x of pieces on `b_row`, into a second zone. zmin/zmax[0] from the W-row zone, [1] from the b-row zone (when the b-row zone is empty, use the W-row zone for it).
   - Inputs: cover = `spec.prio_tiles`, x_cap = xI − 2, nmax = the largest n ≤ 10 with `spec.bridge_span_ok(n)`, `shift_back = true`, `k_w = JUNCTION_PRIO_AFTER_MERGE`, `k_sb = JUNCTION_SHIFT_BACK_TILES` (two new file-level `constexpr int` with doc comments citing D145 and the pinning test below; start them at the values today's winning candidate implies: k_w from x_lm − x_nw − prio_tiles on the test spec (RECON-lane-geometry: 16 − 14 = 2), k_sb = 1), `x_sb_min = 2`.
   - Solve for stagger 0 and stagger 1; for each feasible solution build lanes (3), then pass 2 with `under_spans` from each lane's own heads, N->W goals on `b_row` in [x_sb + 1, x_nw], and the W->S start `x_sl + 1`. Keep the candidate that is clean with the lower `ScoreJunctionLayout`; prefer stagger 1 on a tie.
   - For style 1 the parameters `bridge_margin`, `w_slack`, `gap` and `sb_back` are ignored (leave the sweep loops alone; S2f removes them). `BuildJunctionCandidate` for style 1 calls `BuildJunctionCandidateWithGap` once (no tight/loose, no sb_back loop). `e_slack` and the E side are unchanged.
   - Keep the existing checks: h_w ≥ 0, h_e + 1 ≤ width − 1, `spec.bridge_span_ok(n)`.

## Tests (red first, outer first, `[junction][generator]`)
1. Outer: "GenerateJunction: staggered parallelogram, one signal tile per leg (D118)": generate `MakeTestJunctionSpec()`; the layout is clean; find the two ML bridges on the W side; CHECK lane B's heads are one tile +x of lane A's at both ends; CHECK each lane has exactly one signal-legal piece between the lane split and its near head and between its far head and the lane merge, and it carries a signal; CHECK `ScoreJunctionLayout(...).first` (tiles) ≤ the value at your worktree's head (measure it first, put the number in the test with a comment). Red today: heads equal.
2. "Parallelogram solver constants are minimal (D145)": with `MakeTestJunctionSpec()`, for each of k_w and k_sb: building with the constant − 1 gives no clean style-1 candidate, and the reason names the rule (collect `CheckJunctionRules` violations of the best rejected candidate; CHECK the expected rule: G9c/G9b for k_w; for k_sb whichever rule rejects — record it). Expose the two constants for the test only through a test-visible seam you name in the report (e.g. an optional override in `JunctionCandidateParams`); if no seam is possible without widening scope, stop. If constant − 1 is clean, lower the constant until it is not and report the new value.
3. The inner tests of changes 1-2 as stated there.
4. All existing tests: style-1 tests whose pinned values change (sizes, tile counts, positions) may be updated to the new values; list every change. A test that asserted the unstaggered form (equal heads, the `gap`) is updated to D118 only if the change follows from D145; list it.

## Do not
- Change style 0, the E side, e_slack, the router, the checker, the signal pass, or remove the sweep loops.
- Add style-0 support to the solver.

## Stop conditions
- The test spec no longer generates a clean style-1 layout.
- The checker rejects a head directly after a bend (report the violation): D110 says it is legal.
- More than 3 existing tests fail for reasons other than pinned values.
- The full suite gets more than 20% slower on an idle machine (compare single cases "GenerateJunction reports the hub offset" and "GenerateJunction: the W side is tight", before and after, 3 runs each).

## Gates
1. Build `build-rel` `openttd_test`: no errors, no new warnings.
2. `ptest.sh build-rel/openttd_test "[junction]~[slow]" 8`, then `"~[.]"`.
3. Commit: `Change: Junction parallelogram lanes from the skeleton solver, staggered heads`, body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Commit only your files.

## Report
- References read
- Commit: hash and subject
- Red evidence per test (the failure message seen before the code existed)
- Types, functions, members, constants added or removed: names only
- Suite results and wall times
- Stops, and anything left undone
- State: `git status --porcelain` and `git log -1`

When you have reported, stop.
