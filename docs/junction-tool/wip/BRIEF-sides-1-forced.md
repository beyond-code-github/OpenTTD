# BRIEF sides-1 (measurement): how many keep-apart sides does Shape's layout force, and are the forced sides always right?

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 90 minutes wall. No solves. A read-out addition (no behaviour change) and a hidden measurement test; the wide gate once.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- slots-1 (`docs/junction-tool/wip/probes/slots/two-stage.md`): with every keep-apart side a choice (4 binaries per wall pair, ~85% of columns) no hub solves in 15 s, not even T1. D431: sides by construction where Shape's layout decides them, binaries elsewhere. The question: what fraction of pairs does the layout force, and is the forced side always the one legal drawings have?
- Candidate rules already in planar_draw.cpp (used so far only with a reference drawing `sides_from`): `sides_by_walk` (a pair whose face walk is monotone towards one wall's inward normal takes that candidate), `walk_strict` (D417: only when the walk's guaranteed projection, walls between at least their least length, reaches the spacing), `facing_sides` (D426: walk monotone both ways; anti-parallel facing walls), `inward_sides` (candidates restricted to inward normals).
- Read first: planar_draw.h ~110-125 and ~185-215 (these options and counters); the pair loop in planar_draw.cpp where each rule picks its candidate; `PlanarKeepApartPair`, `keep_apart_out`, `PlanarKeepApartHolds`; D417, D426, D431; PAPER-notes C10; the domain-suite seam and `RunEncoding` (where both-accept polylines are known).

## 2. Changes (read-out only)
1. `PlanarKeepApartPair` gains, per rule, the candidate it forces (or none): `walk`, `walk_strict`, `facing` (and `inward`'s candidate set as a bit mask). Filled in the pair loop under `keep_apart_out` by the same code the rules use, without needing `sides_from` (factor the rule's candidate choice into a lambda both paths call; no change to what the rows are when the rule options are on). If a rule's choice cannot be computed without `sides_from` (it reads the reference), stop and report which and why.
2. `keep_apart_out` must work in binary mode as today; the new fields are informational.

## 3. Measurement (hidden test `[.][sidesmeasure]`)
For each fixture (X1, X2, M1, K1, L1, Y1, P1, BR1, TT1, TS2) over its configurations, on every polyline the domain accepts (legal) and the judge accepts, and on V's 197 at its own points (the answer-key mapping):
- per rule: pairs it forces (count and share of all pairs), and pairs where the forced candidate FAILS on the legal points (`PlanarKeepApartHolds` with only that candidate): that is the rule refusing a legal drawing (too strict). List the first 5 failures per rule per fixture (fixture, polyline key, pair tag, walls, the forced candidate, its margin).
- the union of `walk_strict` and `facing` (rules that never fail on any legal polyline, if any): forced share.
- 197 separately: forced share per rule and failures.
Report a table: rule x fixture: forced share, failures.

## 4. Gates and commit
- `tools/junction/jt fast build '[planardomain]' 5`, `[planardraw]` 30 (Debug), `tools/junction/jt gate build-rel` once (the intermittent parallel-load crash: report, pass alone).
- One commit: `Add: Junction: keep-apart side rules as read-outs, measured against every legal drawing (D431)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`; the table in `docs/junction-tool/wip/probes/slots/sides-forced.md`.

## 5. Report (short)
References read; commit; the table; the safe rules and their forced share overall and on 197; first failures per unsafe rule; gates; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
