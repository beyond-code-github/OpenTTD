# BRIEF domain-48: TS2's base crosses two lines on one piece; find why, then land TS2

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 75 minutes wall. Never run git stash. Do not touch branch wip-defaults.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- domain-47 (draft patch `/tmp/claude-1000/s/ts2-draft.patch`, TS2 at `@ 2 6 10`): the base drawing is Optimal and domain-legal but fails `ConformityHolds` (planar_domain_encoding.cpp ~821): A3's crossing with B1 is assigned piece 13 (Shape's headings `1 0 1 2 1 0 -1 0 1 0 -1 0 1 0 -1 0 1 0`), the drawing has piece 13 zero-length at (9,0) (A3's end) and crosses BOTH B2 (y 19) and B1 (y 11) on its long piece 9. TS2 is the first fixture with two crossings on one track separated by a flex group.
- Read first: `DrawnFixture` (how the base variant is made: "the flex-stripped variant"), `ConformityHolds`, `ContractedVariant`'s event-piece remapping; in planar_draw.cpp the crossing rows (how a crossing's point is tied to `event_piece`).

## 2. Diagnose (no code change yet)
1. Print, for the base variant DrawnFixture solves: A3's pieces, its `flex`, and `event_piece` for both crossings; and the drawing's A3 points. Does `event_piece` index the same piece list the drawing used?
2. Read the crossing rows the program writes for A3 x B1: which piece's points do they tie the crossing point to, and do they hold on the drawing?
3. Class the cause: (a) a harness bug (event pieces not remapped when DrawnFixture strips flex, or ConformityHolds reading a different piece list): fix it in the test file and say exactly what changed; (b) the program's crossing rows let a crossing slide onto a neighbouring piece through zero-length pieces (a drawing-program gap): do NOT fix; report the rows and stop; (c) anything else: stop and report.

## 3. If (a): land TS2
Apply the draft patch (TS2 fixture and its three tests, as BRIEF-domain-47 §2), with the fix; the fast test, the exactness test (or `[!shouldfail]` with groups in full), the hidden report once in Release. Check every other fixture's base still passes `RequireBaseLegal` and exactness is unchanged.

## 4. Gates and commit
- `tools/junction/jt fast build '[planardomain]' 5` (Debug), `tools/junction/jt gate build-rel` once.
- One commit (only if (a)): `Fix: Junction: TS2 in the domain suite; <the harness fix in a few words> (D376, D436)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 5. Report (short)
References read; the diagnosis (each step's evidence, the class); commit if any; TS2's classes; gates; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
