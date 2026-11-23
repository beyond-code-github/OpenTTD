# BRIEF domain-28: the fixed-point checker solves a grid half from its own link row

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 60 minutes wall. The change is in the fixed-point checker only (a measurement tool; the solver is untouched), so no wide gate. Never run git stash. Do not touch branch wip-defaults.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge. NOTE: `build-rel`'s binary still holds a previous agent's temporary debug output; it rebuilds on the next build (the source is clean).
- domain-27 (diagnosis): with grid on, every forced point variable v is rewritten as `v = 2h + p` (row `grid link`, emitted LAST), and every other row is rewritten over h. `BuildPlanarProgramPlan` (planar_draw.cpp ~232-321) eliminates unknowns from equalities in row order, so for a polyline whose port end is short of the box edge, the rewritten port row `port N edge A1` (`2h = 0`) is used to solve h = 0 and is never tested; the `grid link` row is then tested and blamed. The real failing row (a port row, so the polyline is off interface) is hidden. K1 example from domain-27: `A1:1*1,0*6,;ABC1:2*4,;B1:2*12,;BC1:2*4,;C1:3*1,4*6,;` (A1 ends at y = 2).
- The rule: a variable is solved from its DEFINING row. A grid half h of a point variable is defined by its `grid link` row (h = (v - p) / 2); every other row over h is then a check.
- Read first: `CODINGSTYLE.md`; planar_draw.cpp ~215-400 (`PlanarProgramPlan`, `BuildPlanarProgramPlan`, `CheckPlanarProgramOnPoints`) and ~1895-1975 (the grid rewrite, to confirm the link row's form `{v, 1}, {h, -2}` = p).

## 2. The change
In `BuildPlanarProgramPlan`, the single-unknown pass visits the rows tagged exactly `grid link` before every other row (an order vector built once: link rows first, then the rest in row order; the pair pass is unchanged). Nothing else. Comment on it: `/* A grid half is solved from its own link row (its definition), so a row rewritten over it is checked, not used to solve it (domain-27). */`

## 3. Tests (red first)
fast `[junction][planar][planardomain]`: "planar domain: a port row broken under grid is blamed on the port row, not the grid link (domain-27)": K1 plus the domain-27 example (build its Geometry so the contracted polyline is the one above; the K1 configuration `node0`/neighbourhood that produced it is in the K1 report's `[grid]` too-strict group), the read-out program as the suite builds it with grid on and port rows on; assert `CheckPlanarProgramOnPoints(...).broken` contains a tag starting `port N edge` and contains no `grid link`. Red before the change (record it).

## 4. Runs
Release (`build-rel`), X1, X2, M1, K1, L1, Y1, P1, rule set `span+lead+endnode+crosspiece+grid+portzero`, overwrite each `..._portzero` report. Timeout 300 each. Expected: the `[grid]`-only too-strict groups move to off interface. Every too-strict group that remains, and any too loose, is a finding: give its key and first example in full.

## 5. Gates and commit
- `tools/junction/jt fast build '[planardomain]' 5` and `tools/junction/jt fast build '[planardraw]' 30` (Debug).
- One commit: `Fix: Junction: the fixed-point checker solves a grid half from its link row (domain-27)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 6. Report (short)
References read; commit; red evidence; what changed; per fixture: classes before (domain-26) and after, all seven; remaining too strict/too loose groups in full; gates; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
