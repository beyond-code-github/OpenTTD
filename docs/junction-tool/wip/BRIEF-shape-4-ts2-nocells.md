# BRIEF shape-4 (probe): TS2 with only suite-judged rows (no cell rows)

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 45 minutes wall; each solve capped at 15 s; no rerun loops. Probe only: extend the hidden `[.][shapeprobe]` tests and the notes; no production change.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- shape-2 and shape-3 (`docs/junction-tool/wip/probes/shape/ts2-angle.md`): TS2 at the default port spacing is Infeasible at 90 and 45 degrees, even with flex on every stretch. Those solves ran `RefinePlanarShapeCells` and their conflicts contain `clear`, `room` and `land` rows: cell-era rows retired by D431 and never judged by the domain suite. Slots v2 (D441) is built only from suite-judged rows. So the question is open until the solve uses only those.
- Read first: planar_draw.h options `clearance`, `clear_rows`, `pairwise`, `pairwise_all_faces` (~150-160) and whatever writes rows tagged `clear`, `room`, `land`, `dummy` in planar_draw.cpp (grep the tag strings); the shapeprobe tests.

## 2. Steps
1. For TS2's 90-degree and 45-degree variants, flex placed (flex C, lattice clause as production), NO `RefinePlanarShapeCells`; options as DrawnFixture's but with keep-apart only through `pairwise` + `pairwise_all_faces` and no cell rows. List the program's tag families (`program_out`, `build_only`) and confirm none is `clear`, `room`, `land` or `dummy`; if one remains, find the option that drops it (do not change code: if no option does, stop and report which writes it, file:line).
2. Solve both at the default spacing: status, ms, size, inserts used; if Optimal, the domain's verdict on the points (`PlanarDomainInputOf`).
3. If both Infeasible: the smallest N port spacing (`port N out N @ ...` with 1, 2, 3 tile steps) at which the 45-degree variant is Optimal, one solve each; and the conflict at the default spacing by family deletion (at most 20 solves).

## 3. Output
Append "no cells" to `docs/junction-tool/wip/probes/shape/ts2-angle.md`; commit `Doc: Junction: TS2 with only suite-judged rows (probe)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 4. Report (short)
References read; commit; the tag families; both solves; domain verdict; the smallest spacing if reached; the reading in two lines; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
