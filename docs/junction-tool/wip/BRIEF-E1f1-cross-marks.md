# BRIEF E1f-1: crossing direction marks for PSG 197's `cross` statements (data only)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Repo /home/pete/OpenTTD, branch feature/junction-tool, head HEAD_HASH (check `git log -1`). Main checkout, data only: no build, no tests, one Python process at a time.
- Design: docs/junction-tool/wip/CARD-E1d-psg197-variant.md section "G3 design" (read it). Budget 30 minutes.

## Read first
- docs/junction-tool/wip/probes/e1/order.py (its walks, displaced polylines and intersection points) and NOTES.md (the three hand fixes made when the statements were written: E2/E2b, A1e's D12e/D12f, D12d x D12e dropped).
- The 53 `cross` lines at the end of tools/junction/topologies/psg197.txt.

## What to produce
1. docs/junction-tool/wip/probes/e1/marks.py (reuse order.py's code by import or copy; do not modify order.py): for every pair (L, M) of the 121 in psg197.txt's `cross` statements, the intersection point P of their (displaced) polylines and each line's unit heading there (the segment of that line containing P). For pairs with more than one intersection use the one NOTES.md names (E2 x E2b: the one at y 162, (82.5, 162.0); D12c x D23c: (102.0, 178.5)). For A1e x D12e and A1e x D12f, use their walks' point as computed (they share it; the mark depends only on headings).
   Mark of M in L's statement: `>` when dot(dM, left(dL)) < 0 (M arrives from L's left), `<` when > 0; left(d) = (d.y, -d.x), frame x east, y south (the save's coordinates). A dot within 1e-9 of 0 (parallel at P) is a stop.
2. Rewrite each `cross L: M1, M2, ...` line of psg197.txt as `cross L: M1>, M2<, ...` (the mark appended to each partner, no space; order unchanged; nothing else in the file changes).
3. docs/junction-tool/wip/probes/e1/marks-out.txt: per pair, P, dL, dM, both marks.

## Checks (a breach is a stop)
- For every pair, the mark of M in L's statement and the mark of L in M's statement are opposite (one `>`, one `<`).
- `git diff tools/junction/topologies/psg197.txt` changes only the 53 `cross` lines, and stripping the marks gives the old lines byte for byte.
- `python3 /home/pete/OpenTTD/docs/junction-tool/wip/probes/e1/parity.py /home/pete/OpenTTD/tools/junction/topologies/psg197.txt /home/pete/OpenTTD/docs/junction-tool/wip/probes/e1/cross-pairs.md` still prints `bad 0`.

## Commit
`Doc: Junction E1f-1: PSG 197 crossing direction marks from the save's geometry (D389)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`; only marks.py, marks-out.txt, psg197.txt. Do not push.

## Report (short): references read; commit; counts of `>` and `<`; the checks; any stop; `git status --porcelain`, `git log -1 --oneline`. When you have reported, stop.
