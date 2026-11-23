# BRIEF lazy-3: T2 through the aligned Slots path

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 75 minutes wall; each solve capped at 15 s; at most 12 rounds per stage; deletion filters at most 30 solves. Test code only.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- lazy-2 (c7b61f3909, `docs/junction-tool/wip/probes/slots/lazy.md`): with `SlotsOptions` (the judge's rule set) plus corridors, T1 is drawn from Shape's variant with lazy keep-apart rows, and the judge and the domain accept it. In lazy-1, T2 failed: stage A's round 1 (44 pairs) hit 15 s; stage B was Infeasible even with no keep-apart rows; offers 3 + D443 was Infeasible with no rows. Those runs used mismatched options.
- Read first: lazy.md; the `[.][lazymeasure]` test as lazy-2 left it (`SlotsOptions`, the references, the turn check, `fb.options.corridor = true`).

## 2. Changes and steps
1. `SlotsOptions` sets `corridor = true` (D421: copies and prio rows need the corridor family; the judge's read-outs then get it too): the exactness tests must stay 0/0 (corridor is already on for the DrawnFixture configurations; X1Fixture-based ones have no beside tracks; confirm), and remove the one-off line in the lazy test.
2. Stage A also uses `SlotsOptions` (plus the stage's own: uncontracted flex, integer, grid as SlotsOptions has it, `only_pairs`).
3. T2 (offers 1/D443 off, then offers 3/D443 on): stage A and B as lazy-2. Record rounds, pairs with rows, ms, verdicts.
4. Any Infeasible round: the minimal conflict by deletion over row families, then within the last family over tags (at most 30 solves), with a plain reading of what it means geometrically. A stage-B infeasibility with no keep-apart rows means stage A's S-bend choice (its contracted writing) cannot be drawn: say which track's writing and why if the conflict shows it.
5. If stage A's round with the failing pairs times out: report the pair count and the binaries; try once with the side of each added pair fixed from the previous round's drawing (`sides_from`, the read-out's best candidate) and report status/time.

## 3. Gates and commit
- `tools/junction/jt fast build '[planardomain]' 5`; `tools/junction/jt gate build-rel` once (intermittent parallel-load crashes: rerun the failing tests alone and report).
- One commit: `Test: Junction: T2 through the aligned Slots path (lazy-3)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`; results appended to lazy.md.

## 4. Report (short)
References read; commit; T2's rounds and verdicts; conflicts and their reading; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
