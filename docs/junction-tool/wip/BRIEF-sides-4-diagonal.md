# BRIEF sides-4: the remaining walk_strict failures (margin -1, diagonal walls)

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 60 minutes wall. No solves. Fix only a measurement or units bug in the rule's read-out computation (`guaranteed`/`monotone`/`walk_choice`), proven by the hand checks; no other change.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- sides-3 (11f3225914; `docs/junction-tool/wip/probes/slots/sides-forced.md` top table): with walls cut at crossings, walk_strict failures fell (197 85 -> 10, X1 117 -> 21) but remain on X1 (21), L1 (157), Y1 (2), BR1 (3), 197 (10). Nearly all have margin -1 and involve diagonal walls (e.g. X1 `sep 1 2 4`, walls B1/A1 pieces 1/0, B1 `0*2,1*2,0*10`, candidate 0, margin -1; L1 `sep 1 1 3`, A2/B1 pieces 0/0, A2 `1*1,0*6`, candidate 0, margin -1). A uniform -1 suggests a units issue (a diagonal piece of length L moves L half tiles on each axis; an axis piece of length L moves L half tiles on one) in the rule's guaranteed projection or in the pair's spacing for diagonal walls, not a wrong side.
- Read first: sides-forced.md; the pair loop's `guaranteed`, `monotone`, `walk_choice`, the spacing per candidate (`RungSpacing` units) and the candidate rows; `PlanarKeepApartHolds`/`PlanarKeepApartEnd`.

## 2. Steps
1. Hand-check X1 `sep 1 2 4` and L1 `sep 1 1 3` (the examples above): walls' endpoints on the points, the walk between them (walls, headings, least lengths), the rule's guaranteed projection and the spacing it compared, the candidate's row and its value on the points. Find exactly where the 1 is lost.
2. Classify: (a) a units/rounding bug in the rule's computation (the rule certifies a side whose row cannot hold by construction): fix it, and the fix must make the rule more conservative (it may certify fewer pairs), never less; (b) the rule is right and the row is too strict (the legal drawing sits closer than the spacing on that candidate while the domain accepts it): no fix, describe; (c) other: stop.
3. If (a): rerun the sides measurement and the 197 read; the new table. All exactness tests unaffected (the read-out does not change rows; confirm).

## 3. Gates and commit
- `tools/junction/jt fast build '[planardomain]' 5` (Debug); `tools/junction/jt gate build-rel` once only if production code changed.
- One commit: `Fix: Junction: walk_strict's guarantee for diagonal walls (sides-4)` if (a), else `Doc: Junction: walk_strict's diagonal failures checked (sides-4)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 4. Report (short)
References read; commit; the two hand checks and the class; the new table (if any); stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
