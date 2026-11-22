# BRIEF domain-3c: judge too loose per polyline too

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 30 minutes wall. Test code only. No rerun loops. Never run git stash.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- The harness (`src/tests/junction/planar/planar_domain_encoding.cpp`) judges too strict per polyline (some representation accepted, by the backtracking search), but too loose still per enumerated representation. With `+lead` almost every enumerated representation fails a lead row (the base puts A1's and B1's runs on piece 4, not piece 0), so nothing is accepted and too loose reads 0 for a reason that says nothing about the rules. The solver chooses the representation, so too loose is also a question about the polyline: rows are too loose on an illegal polyline when **some** representation of it is accepted.

## 2. The change (test file only)
- Classify per **polyline** (cache by key), not per enumerated geometry, with the existing search deciding "rows accept the polyline" (same caps: 20000 representations per track, 50,000 nodes, else undecided):
  - domain accepts, rows accept: both accept; domain accepts, rows reject: too strict; domain rejects, rows accept: too loose; domain rejects, rows reject: both reject; undecided as now; off grid as now (per geometry, before the polyline).
- Report counts per configuration in polylines (and, in brackets, the enumerated geometries they came from). Remove the "both accept (other representation)" class.
- Too-loose examples: the polyline, the domain breaks (as now), and the accepted representation's piece lengths per track (the search's witness), plus the keep-apart pairs between each break's tracks with their skip reason or "held" on the witness, and the lead rows of those tracks.
- Too-strict examples as now.

## 3. Gates and output
- `tools/junction/jt fast build '[planardomain]' 5`; `tools/junction/jt gate build-rel` (one rerun allowed if only the LL² joint case fails; report it).
- Release, the eight sets as in domain-3b (same file names, overwritten), timeout 120 each; a timeout is recorded and the loop goes on.
- One commit: `Change: Junction: the encoding report judges too loose per polyline as well (D428)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 4. Report (short)
Commit; per rule set one line in polylines: both accept, both reject, too strict, too loose, undecided (and off-grid geometries); for `plain+lead` and `span+lead` the too-strict and too-loose group keys with counts and the first example of each (polyline, breaks or failing rows); wall times; suites; state. When you have reported, stop.
