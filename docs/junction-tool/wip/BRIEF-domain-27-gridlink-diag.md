# BRIEF domain-27 (diagnostic): why `grid link` breaks on legal polylines once port rows are on

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 40 minutes wall. Diagnosis only: NO commit; every edit is temporary and reverted at the end (`git checkout -- src`). Never run git stash.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`.
- domain-26 turned the port rows on in the domain suite. Since then, polylines the domain accepts fail ONLY the program row `grid link` (K1: 20 polylines, X1: 19). Example (K1 report `docs/junction-tool/wip/probes/domain/encoding-k1-span_lead_endnode_crosspiece_grid_portzero.txt`, the `[grid]` too-strict group's first example): `A1:1*1,0*6,;ABC1:2*4,;B1:2*12,;BC1:2*4,;C1:3*1,4*6,;`. The X1 one is the first `[grid]` example in the X1 report.
- `grid link` rows come from the whole-tile pass in planar_draw.cpp (~1897-1975): parities forced by integer equalities from the grid's seeds, each forced variable written as `2 h + p`. A link breaks on fixed points only if a variable's forced parity is wrong for that drawing.

## 2. What to find (for the K1 example, then the X1 example)
1. Build the read-out exactly as the suite does for that polyline (reuse the suite's code path; a temporary test or a temporary env hook is fine).
2. List every broken `grid link` row: the variable (track, point index, axis, or box width/height or other; use the program's variable names if it has them), its forced parity p, its value on the points.
3. Trace the propagation for each: the chain of rows (tags) from a seed or an edge row to that variable, with each row's terms. Temporary debug output in the propagation loop is fine.
4. Say which row in the chain is false for this drawing (a legal drawing that breaks a row the propagation trusted), and whether the same chain exists with port rows skipped (it should not, since these polylines were accepted then).

## 3. Report (short)
For each example: the broken links (variable, p, value), the chain, the false link in it, in plain words (e.g. "the N edge row y = 0 makes A1's end even; the piece row of A1's diagonal then forces ..."). No fix. State that `git status --porcelain` is clean at the end. When you have reported, stop.
