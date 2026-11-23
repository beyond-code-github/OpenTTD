# BRIEF domain-45: a prio fixture with room, and the prio length as a behaviour (D384)

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 60 minutes wall. Test code only. Never run git stash. Do not touch branch wip-defaults.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- domain-38 stopped: in P1 the lane A1#3 (row split to merge) cannot reach `rules.coverage` = 32 half tiles, because C1's S port fixes the merge's x near A1's port. D384: "A prio row is exactly its coverage long along its path ... the pair (lane and row) runs exactly the coverage between the row's split and merge".
- Read first: DECISIONS D384, D410 (port positions `@`), D421, D436; `BRIEF-domain-38-prio-and-leaks.md` §2 (the two tests as specified there); the P1 fixture and its tests; `DrawnFixture`; the seam.

## 2. Changes
1. Fixture P2: P1's text with C1's S port positioned far east with D410's syntax, e.g. `port S in N @ 20: C1` (tiles; choose the smallest value that lets the lane reach coverage + 2 = 34 half tiles in a connected on-grid geometry, and state it), everything else as P1 (`straight AC1: A1`, `prio A1`), `DrawnFixture` with `least_extra = 2`. A fast test "planar domain: P2 loads, has a prio row and its base is legal", and its exactness test in `[planardomainexact]` (neighbourhood configurations) plus its hidden report (`PLANAR_DOMAIN_REPORT_P2`). If P2 is not exact, tag the exactness test `[!shouldfail]` and report the groups in full.
2. Test A (normal) and Test B (known failure, `[!shouldfail]`) exactly as BRIEF-domain-38 §2 specifies, on P2: the lane's travel between the two row nodes (along A1#3's points) = coverage (accepted by judge and domain) and = coverage + 2 (domain accepts; the judge must refuse with `prio length`, which no implementation does yet).

## 3. Gates and commit
- `tools/junction/jt fast build '[planardomain]' 5` (Debug), `tools/junction/jt gate build-rel` once (report its time; over 30 s: stop with the timings).
- One commit: `Test: Junction: a prio fixture with room; the prio length as a behaviour (known failure) (D384, D436)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 4. Report (short)
References read; commit; P2's text and the port position chosen; base size; P2's classes; the two travels and test B's status; gates; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
