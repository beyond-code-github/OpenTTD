# BRIEF domain-3b: judge each polyline by all its representations

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 60 minutes wall. Test code only. Each harness run must finish in under 60 s Release; eight runs. No rerun loops.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>` (given at launch). Commit there; do not merge.
- The harness (`src/tests/junction/planar/planar_domain_encoding.cpp`, hidden `[planardomainenc]`) enumerates piece lengths. One track polyline has many piece-length representations: a straight run can sit on any of the pieces of its heading, with the others (and the flex inserts between them) at 0. The drawing program chooses the representation, so:
  - **too loose** is exact per representation (the solver could pick an accepted representation of an illegal polyline): keep counting it per representation.
  - **too strict** must be judged per polyline: a domain-legal polyline is too strict only when **no** representation of it is accepted.
- This step changes only how too strict is judged and reported.

## 2. Read first
- The harness file, whole. `PlanarKeepApartHolds`, `PlanarPointRowHolds`, `PlanarNodeLeadRows` in `src/junction/planar/planar_draw.h`.

## 3. The change (test file only)
1. **Polyline of a geometry, per track:** walk its pieces; drop zero-length pieces; merge consecutive pieces with equal heading mod 8 into one run (heading mod 8, summed length). The geometry's key is all tracks' run lists.
2. **Representations of a polyline on a track's piece list** (static helper `Representations(pieces, runs, cap)`): every assignment of a non-negative length to each piece such that walking the pieces in order, dropping zeros and merging equal headings (mod 8), gives exactly `runs`; each piece's length a multiple of 2 on an even heading and of 1 on an odd heading. Enumerate by depth-first walk over the pieces with state (run index, length left in the run): a piece whose heading mod 8 equals the current run's takes any allowed amount from 0 to what is left; any other piece takes 0, except that when the current run is used up the walk may move to the next run at that piece. Every run must be used up by the end. Stop at `cap` results and say so.
3. **Rows accept a polyline** (rule set as now, `+lead` included): when the product over tracks of their representation counts is at most 20000, try every combination (tracks varied independently, all others at that combination) and accept if one passes every keep-apart pair (and every lead row with `+lead`); else class **undecided** (report the count). Build the read-out once per configuration as now; this needs no solve.
4. **Classes:** per enumerated geometry as now for both accept, both reject, too loose and off grid; but a geometry the domain accepts and whose own representation fails is now checked by step 3 on its polyline (cache per polyline key): passing means **both accept (other representation)**, failing means **too strict**, over the cap means **undecided**. Report the distinct polyline count for too strict and for too loose too.
5. Too-strict examples show the polyline (per track its runs) and, for the representation the enumeration gave, the failing pairs and lead rows as now.

## 4. Test (fast, `[junction][planar][planardomain]`): "planar domain: representations of a polyline"
Hand pieces with headings [2, 1, 2, 3, 2]. Runs [(2, 4)]: exactly 6 representations (2 tiles over pieces 0, 2 and 4 in whole tiles; pieces 1 and 3 always 0). Runs [(2, 2), (3, 1), (2, 2)]: exactly 2 (the first run on piece 0 or on piece 2; piece 1 = 0, piece 3 = 1, piece 4 = 2). Red: a stub returning nothing fails the counts.

## 5. Do not
- No production change. Do not change the enumerated configurations, the grid filter, the too-loose judgement or the rule sets.
- Do not interpret results.

## 6. Stop conditions
- A harness run over 60 s, or more than 5% of too-strict candidates undecided: stop and report.
- Anything not covered.

## 7. Gates and output
- `tools/junction/jt fast build '[planardomain]' 5` (Debug); `tools/junction/jt gate build-rel`.
- Release, for each of `plain`, `span`, `zero`, `all`, `plain+lead`, `span+lead`, `zero+lead`, `all+lead`: `PLANAR_DOMAIN_RULES=<set> PLANAR_DOMAIN_REPORT=/home/pete/OpenTTD/.claude/ng1/docs/junction-tool/wip/probes/domain/encoding-x1-<set, + as _>.txt timeout 120 build-rel/openttd_test "[planardomainenc]" > <scratch log> 2>&1` (the `all` run writes `encoding-x1-all.txt`; replace `encoding-x1.txt` by it: delete `encoding-x1.txt`).
- One commit: `Add: Junction: the encoding report judges too strict per polyline over its representations (D428)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 8. Report (short)
Commit; red evidence; per rule set one line: both accept, both accept (other representation), both reject, too strict (geometries / polylines), too loose (geometries / polylines), undecided, off grid; the too-strict and too-loose group keys with counts for `span+lead` and `plain+lead`; wall times; suites; stops; state. When you have reported, stop. Never run git stash.
