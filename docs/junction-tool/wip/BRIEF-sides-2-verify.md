# BRIEF sides-2: verify the walk_strict failures, and measure on more legal drawings

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 60 minutes wall. No solves. Fix only a measurement bug (if found) in the read-out or the measurement test; no rule change.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- sides-1 (`docs/junction-tool/wip/probes/slots/sides-forced.md`): `walk_strict` forces 30-77% of pairs but "always forces candidate 0 and the legal drawing keeps the other side" on six fixtures and 197. A real rule error would not be uniform; this pattern suggests the read-out's candidate index does not mean what the rows' candidate index means (e.g. the read-out fills `walk_strict` with a position in a list that differs from `PlanarKeepApartHolds`'s candidate numbering).
- Also: the measured drawings are few (X1 2, X2 1, L1 1, TS2 1) because only polylines the judge accepts WITH interface rows were used; off-interface polylines (domain accepts, judge accepts without port/box rows) are legal too.

## 2. Steps
1. Hand-check X1 `sep 4 3 6` (A1/B1 pieces 3/0, margin -6): from the polyline in sides-forced.md, the two walls' endpoints, every candidate (axis, side, spacing) as the pair loop builds them, the walk the rule takes, which candidate the rule logic picks, which index the read-out stores, and which candidate `PlanarKeepApartHolds` tests for that index. Say whether the read-out stores what the rule picks, in the numbering the check uses.
2. If it is a mapping bug: fix it in the read-out (or the measurement test), and say exactly what. If the rule itself picks the side the legal drawing does not keep: say why from the walk (which wall's inward normal, what the walk's guaranteed projection was), no fix.
3. Rerun the measurement with off-interface drawings included (a polyline counts when the domain accepts it and the judge accepts it with `interface_rows = false`), on all ten fixtures; 197 as before (its caveat: V's points carry 6 rebuild artefacts, so its failures are only indicative).
4. Update the table in sides-forced.md (keep the old table under a heading "sides-1 (before the check)").

## 3. Gates and commit
- `tools/junction/jt fast build '[planardomain]' 5` (Debug); the wide gate only if production code changed.
- One commit: `Fix: Junction: <what> (sides-2)` if a bug was fixed, else `Doc: Junction: walk_strict failures checked (sides-2)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 4. Report (short)
References read; commit; the hand check in a few lines and its verdict; the new table; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
