# BRIEF domain-52: finish D438 on V's 197: the deck and order rows at an along crossing

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 60 minutes wall. Diagnose first; change code only under case (a) below; the wide gate once if you change code. Never run git stash. Do not touch branch wip-defaults.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- D438/D439 (fe7e286f5d): along mode. On V's 197 (hidden test `[.][planardomain197]`, judged at V's points) the keep-apart refusals at L40/L57 and L89/L74 are gone; three rows still refuse: `deck 1`, `deck 20`, `order L57 0`. `deck <s>` (planar_draw.cpp ~1278): consecutive crossings of structure s along its separator keep `deck_gap` (parallel crossers) or 1 apart (by their crossing parameters). `order <t> <piece>` (~1181): crossings on one piece of track t keep their event order (by parameter).
- Read first: D438, D439, D415 (deck gaps); planar_draw.cpp the `along_from` code (rows `along <c> on/clear`), ~1170-1185, ~1255-1290; the 197 hidden test and its env dumps (`PLANAR_DOMAIN_JUDGE_197` writes refusing rows with terms and values).

## 2. Diagnose
For each of the three rows: its terms and their values on V's points, the structure/track and crossings it names, whether any of those crossings is in along mode (from `AlongFrom`), and V's tiles there (`docs/junction-tool/wip/probes/domain/triage197.py`). Class each:
- (a) ALONG CONSEQUENCE: the row reads a crossing's single parameter, but the crossing is in along mode (the crosser runs along the separator, so its crossing is a stretch, not a point), and V's geometry is legal: the row must treat the along crossing by its stretch (e.g. the deck gap measured to the nearer end of the along stretch, the order by the stretch's start). Implement exactly that for along-mode crossings only, as the narrowest change to those two row families; pinned behaviour test on a fixture where an along crosser is next to a second crosser of the same tunnel (TS2-shaped, or X2 if it has one; if none can be built in the suite, say so and pin only the 197 result).
- (b) ARTEFACT: V's points there are the rebuild's (name it).
- (c) PLANNER RULE / other: describe and stop.

## 3. Gates and commit (only if (a))
- `tools/junction/jt fast build '[planardomain]' 5`, `[planardraw]` 30, `tools/junction/jt gate build-rel` once; all eleven exactness tests 0/0; the 197 hidden run.
- One commit: `Fix: Junction: deck and order rows read an along crossing by its stretch (D438)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`; else commit only notes `docs/junction-tool/wip/probes/domain/along-deck-order.md` with `Doc: Junction: ...`.

## 4. Report (short)
References read; each row's class with evidence; commit; 197 refusals after; gates; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
