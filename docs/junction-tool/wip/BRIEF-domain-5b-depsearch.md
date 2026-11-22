# BRIEF domain-5b: check each program row as soon as its tracks are placed

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 60 minutes wall. No rerun loops. Never run git stash.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- domain-5 judges the whole program on fixed points (`CheckPlanarProgramOnPoints`, its cached `PlanarProgramPlan`), but the harness's polyline search checks it only at a full assignment, so it backtracks blindly and 231 of the polylines under `span+lead` end undecided at the node cap. Too loose = 0 is not proven while illegal polylines can hide among them.
- Each checkable row depends on a set of tracks: the tracks of the points that reach it through the plan's steps. Checked as soon as that set is placed, the program prunes the search like the keep-apart pairs do.

## 2. Read first
- `PlanarProgram`, `PlanarProgramPlan` and `CheckPlanarProgramOnPoints` (planar_draw.h/.cpp); the harness's search.

## 3. Changes
- `PlanarProgramPlan` (production, planar_draw.cpp/.h as it is now) gains, per step, the set of tracks it depends on (a point variable depends on its track; a determined variable on the union of its step's known inputs), and per checkable row its track set. Built with the plan.
- A partial evaluation entry: `PlanarProgramCheck CheckPlanarProgramOnPoints(const PlanarProgram &program, const std::vector<std::vector<std::array<int, 2>>> &points, const std::vector<bool> &placed)` (overload): only the steps and rows whose track sets lie within `placed` are evaluated; the rest are skipped silently. The existing two-argument form is the overload with every track placed.
- Harness search:
  - Unary filter: also drop a track's representations that break a row whose track set is exactly that track (placed = that track only; other tracks' points at the enumerated geometry's, unused).
  - At each depth, after placing a track, call the partial check with `placed` = the tracks placed so far, and fail the node when it breaks a row whose track set contains the new track. Count it as one node.
  - A full assignment then needs no separate program check.
- Report the undecided count per set; keep the caps.

## 4. Test (fast, in the harness file)
- "planar draw: the program checked by track sets": on the X1 base, with `placed` = {B1} only, `end 0` and `cross 0` are not evaluated (their sets hold A1); with every track placed the two-argument and full partial forms give the same `broken` on the base and on test 2's `end 0` geometry. Red: an overload that ignores `placed` fails the first part.

## 5. Gates and output
- `tools/junction/jt fast build '[planardomain]' 5`; `tools/junction/jt gate build-rel` (one rerun if only the LL² joint case fails).
- Release `plain+lead` and `span+lead` (overwrite the probes/domain files), timeout 120.
- One commit: `Change: Junction: the encoding search checks program rows by track set (D428)`, with the Co-Authored-By line for Claude Sonnet 5.5.

## 6. Report (short)
Commit; red evidence; per set the totals line in polylines (both accept, both reject, too strict, too loose, undecided); new group keys with counts and first examples; run times; suites; state. When you have reported, stop.
