# BRIEF domain-43: two configurations from V's 197: a crossing stack and tunnel under tunnel

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 75 minutes wall. Test code only. Record findings; fix no rule. Never run git stash. Do not touch branch wip-defaults.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- Every suite configuration has one crosser per structure, and none has two structures crossing. V's 197 has tunnels with many crossers (D376: crossers under one span run parallel) and tunnels crossing tunnels by depth (D390 (2); the domain accepts it since domain-40). Each new case is a configuration with its exactness asserted (D436).
- Read first: DECISIONS D376, D390, D436; in `src/tests/junction/planar/planar_domain_encoding.cpp`: the BR1 fixture (`DrawnFixture`, `least_extra = 2`, its `Configs` function, its fast "loads ... base is legal" test and its `[planardomainexact]` test) as the pattern to copy; X1's topology text (the `cross` statements with marks) for authored crossings.

## 2. Fixtures (texts are a starting point; a refusal by the parser, the structure loader or Shape is a STOP: report its message and the text you tried)
- **TS2, two crossers under one tunnel:** lines A1 W->E (port W in E, port E out E), B1 and C1 S->N (one port S in N: B1, C1; one port N out N: B1, C1), A1 crossing B1 and C1 in that order, structure `tunnel T1 A1: B1, C1` plus any `straight` statements the loader requires. Write the `cross` statements as X1 does for each pair.
- **TT1, tunnel under tunnel:** lines A1 W->E and B1 S->N crossing once; structure `tunnel T1 A1: B1` and `tunnel T2 B1: A1` (each the other's separator; D390 (2)). If the loader refuses two tunnels on one crossing, stop and report its message.
- Both: `DrawnFixture` with `least_extra = 2`, neighbourhood configurations (k = 3), report envs `PLANAR_DOMAIN_REPORT_TS2` / `_TT1`, report files `encoding-ts2-...` / `encoding-tt1-...` with the default-rules suffix the other reports use.

## 3. Tests
- fast: "planar domain: TS2 loads, its tunnel has two crossers and its base is legal"; "planar domain: TT1 loads, two tunnels cross and its base is legal" (the domain accepts the base: TT1 exercises domain-40's tunnel-under-tunnel rule).
- `[planardomainexact]`: "planar domain: TS2 is exact under the default rule set", same for TT1, asserting 0 too strict and 0 too loose. If a fixture is NOT exact, do not assert it: tag that test `[!shouldfail]` with a comment naming the finding, and report the groups in full (first example each). That is the finding we want.

## 4. Runs and gates
- Release once per fixture (hidden report tests), timeout 300; `tools/junction/jt fast build '[planardomain]' 5` (Debug); `tools/junction/jt gate build-rel` once (report its time against 30 s; if over, stop and report the timings).
- One commit: `Test: Junction: a crossing stack and tunnel under tunnel in the domain suite (D376, D390, D436)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 5. Report (short)
References read; commit; the texts as loaded; Shape's headings; base sizes; per fixture classes and every too strict / too loose group with first example; gate time; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
