# BRIEF shape-2 (probe): does TS2 draw at one-lane port spacing with 45-degree crossings?

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 45 minutes wall; each solve capped at 15 s; no rerun loops. Probe only: a hidden test `[.][shapeprobe]` and a notes file, committed. No production change.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- TS2 (three lanes, `tunnel T1 B1: A2, A3`, `tunnel T2 B2: A3`; texts in the TS2 fixture of `planar_domain_encoding.cpp`): with the default (one-lane) W/E port spacing the base drawing is Infeasible (`docs/junction-tool/wip/probes/domain/ts2-diag.md`): A3 leaves lane 3 on a diagonal and crosses lane 2 at 90 degrees, with no room in one tile. The recon (`docs/junction-tool/wip/probes/shape/recon-steps.md`, TS2 section) found no Shape constraint chose 90 degrees: crossing at 45 degrees (angle a = 1) and turning after the last crossing costs the same turning.
- Question: with A2's and A3's crossings at 45 degrees (headings 1 at their crossings of B1/B2, turning to 0 after their last crossing), is TS2's base drawable at the default port spacing?

## 2. Steps
1. Take Shape's TS2 variant (`ShapePlanarStructureVariant`); print A2's and A3's headings and the angle at each crossing.
2. Build the alternative variant: A2's and A3's headings rewritten so each crosses B1/B2 at heading 1 (45 degrees) and turns to 0 after its last crossing (keep each stretch monotone and the node rule: a diverging leg exactly one eighth off its trunk), via `LoadPlanarShapeVariant` with a shape text (the X1 fixture shows the format) or by editing the variant's pieces and event pieces directly, whichever the code supports cleanly; run Shape's oracle checks on it (`CheckPlanarShapePromise` or the H-checks the loader runs) and report any refusal.
3. Draw both variants at the DEFAULT port spacing (no `@`), with the DrawnFixture options (integer, grid, tidy, corridor, the default rules), `least_extra = 0`: status, ms, size; if Optimal, the domain's verdict on its points.
4. If the 45-degree variant is Infeasible, its conflict (deletion filter over row families as in ts2-diag.md, at most 30 solves).

## 3. Output
`docs/junction-tool/wip/probes/shape/ts2-angle.md` (headings of both variants, the solves, the reading); commit `Doc: Junction: TS2 at 45 degrees (probe)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 4. Report (short)
References read; commit; both variants' crossing angles; both solves' status; the domain verdict; the reading in two lines; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
