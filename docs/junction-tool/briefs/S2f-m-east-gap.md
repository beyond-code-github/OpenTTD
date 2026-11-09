# S2f-m: measure which rule sets the E-side gap (no code change, no commit)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/crash` (detached at bf09d84efb; a diagnostics worktree). You are the only writer there. Never touch sibling worktrees or the main worktree `/home/pete/OpenTTD`.
- Build: `build-rel` is configured (clang-18, ccache): `cmake --build build-rel --target openttd_test -j 8 > s2fm-build.log 2>&1`.
- Run single cases directly: `build-rel/openttd_test "[.s2fm]" > s2fm-out.txt 2>&1`.
- Hardware note: this machine's CPU is unstable under heavy all-core load (diagnosed today). If a run segfaults or SIGILLs, rerun it; report how often.
- Decisions: `/home/pete/OpenTTD/docs/junction-tool/DECISIONS.md` rows D108, D137, D145, D146.

## Why
The E-side window for the N→E merge starts at `x_s + cover + 1 + e_slack` (`src/junction_generator.cpp` ~line 428; `x_s` is the E→S split's first piece x, `cover` is `spec.prio_tiles`). PLAN-S2 E1 wants that `1 + e_slack` to become a distance derived from a rule. The first S2f measured the least e_slack that gives a clean parallelogram (style 1) candidate: 1 on `MakeTestJunctionSpec()` (prio 14, strict), 0 on `PinnedTestSpec()` (prio = tl, not strict); a probe at TL5/prio 15 needed 2. We need to know which rule rejects one tile less, and what it depends on.

## Read first (once each, offset/limit)
- DECISIONS rows D108, D146.
- `src/junction_generator.cpp` 395-440 (E side of pass 1), 712-750.
- `src/tests/junction_generator.cpp` 28-70 (`PinnedTestSpec`), 1205-1245 (how the D145 test collects rejecting rules).
- `src/junction_rules.h`: the `JunctionRule` enum and `CheckJunctionRules`.

## Do
Write one temporary test case tagged `[.s2fm]` at the end of `src/tests/junction_generator.cpp`. It prints (Catch2 `WARN` or `std::cout`) and asserts nothing. For each of four specs:
- A: `MakeTestJunctionSpec()` (as is)
- B: A with `prio_strict = false`
- C: `PinnedTestSpec()` (prio_tiles = tl, not strict)
- D: C with `prio_tiles = 14` (A's coverage, not strict)

For each e_slack 0..3, over heights ml_gap+5..ml_gap+20 and both crossing costs, with `lane_style = 1` and `w_slack = 0`:
1. Count clean candidates (`BuildJunctionCandidate(..., &clean)`), and record the best clean `ScoreJunctionLayout`.
2. For every candidate that builds but is not clean, collect `CheckJunctionRules` violations. Print a histogram: rule → number of candidates it rejects. Then print the first two violations' full text (rule, message, tile) for the lowest height that builds.
3. For the lowest-height built candidate at e_slack = e_min − 1 (if e_min > 0) and at e_min, print: `spec.prio_tiles`, `spec.tl`, `spec.reference_speed`, x_s and the x where N→E actually joins row 0. To get them, read the built layout: x_s is the E→S split; the N→E join is the merge onto the E track's row. Find both by the method the generator uses (the first piece of the E→S raster, the goal piece of N→E). If you can't recover them from the layout without new code in the generator, print `RenderJunctionAscii(layout)` instead and say so. Do not add hooks to the generator.

## Report (then revert)
- One table per spec: e_slack, clean count, best score, rule histogram.
- e_min per spec.
- Item 3's numbers or renders.
- One paragraph, your reading only: which rule moves e_min between the specs, and whether it follows prio_tiles, strictness or neither. Don't propose a formula.
- Then delete the test case, so that `git status --porcelain` in the worktree shows no modified tracked files (untracked build logs are fine), and report that state.

## Do not
- Change any non-test source, or commit anything.
- Change the specs beyond the four named.

When you have reported, stop.
