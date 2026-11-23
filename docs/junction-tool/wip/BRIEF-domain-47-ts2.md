# BRIEF domain-47: TS2 with port spacing that holds Shape's crossing (a crossing stack in the suite)

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 60 minutes wall. Test code only. Never run git stash. Do not touch branch wip-defaults.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- domain-46 (`docs/junction-tool/wip/probes/domain/ts2-diag.md`): TS2's base is infeasible because the ports put lanes one tile apart while Shape crosses lane 2 at a right angle from lane 3's diverging leg; the base is Optimal with W and E ports at `@ 2 6 10` (4 tiles per lane), with and without the optional rules, at `least_extra` 0 and 2. (That Shape accepts headings the default port spacing cannot hold is a Shape-suite finding; not for this step.)
- Read first: `ts2-diag.md`; the hidden `[.][ts2diag]` test; TT1's fixture and its three tests (the pattern); BRIEF-domain-43 §3.

## 2. Changes
- Fixture TS2: the domain-43 text with W and E ports at `@ 2 6 10` (N port: the same positions if the loader requires positions per side consistently, else default; state it), structure as domain-43 (`tunnel T1 B1: A2, A3`, `tunnel T2 B2: A3`, straights), `DrawnFixture`, `least_extra = 2`.
- Tests as BRIEF-domain-43 §3 for TS2: fast "loads ... base is legal", `[planardomainexact]` exactness (or `[!shouldfail]` with the groups in full if not exact), hidden report (`PLANAR_DOMAIN_REPORT_TS2`) run once in Release.
- Keep the `[.][ts2diag]` test (it documents the infeasible default spacing).

## 3. Gates and commit
- `tools/junction/jt fast build '[planardomain]' 5` (Debug), `tools/junction/jt gate build-rel` once (time vs 30 s; over: stop with timings).
- One commit: `Test: Junction: a crossing stack (TS2) in the domain suite (D376, D436)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 4. Report (short)
References read; commit; TS2's text as loaded; Shape's headings; base size; classes; every too strict / too loose group in full; gate time; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
