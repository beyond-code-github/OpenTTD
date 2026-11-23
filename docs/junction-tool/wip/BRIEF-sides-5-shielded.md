# BRIEF sides-5 (measurement): how many undecided keep-apart pairs can never come close?

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 75 minutes wall. No solves. Measurement code in the hidden `[.][sidesmeasure]` test (and the 197 read) only; no production change.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- sides-4 (`docs/junction-tool/wip/probes/slots/sides-forced.md`): the pair-by-pair rules decide sides safely for 9-64% of pairs on the fixtures and ~22% (walk_strict) + 9% (facing) on V's 197. The rest are choices (4 binaries each). Many undecided pairs may never come close in any legal drawing (shielded by walls between them); such pairs need no row at all. This step sizes that.
- Read first: sides-forced.md; the measurement test and the 197 read (`PLANAR_SIDES_197`); `PlanarKeepApartPair` (its candidates, spacing, the rule fields), `PlanarKeepApartHolds`/`PlanarKeepApartEnd`.

## 2. Measure (per fixture over all legal drawings incl. off-interface, and on V's 197)
Restrict to UNDECIDED pairs: neither `walk_strict` nor `facing` forces a side.
1. Empirical bound: per pair (identified by its tag within a fixture's base variant shape; skip a pair whose tag differs between drawings and say how many), its least margin over all legal drawings, where a pair's margin is the BEST candidate's margin (the distance by which the pair is apart beyond its spacing). Report the share of undecided pairs whose least margin is >= 0, >= 2, >= 4, >= 8 half tiles. (A pair always far apart in every sample is a candidate for pruning; this is an upper bound on what a sound rule can prune, from samples only.)
2. A provable rule, transitivity through a third wall: an undecided pair (i, j) in a face is IMPLIED when the face has a wall k such that both (i, k) and (k, j) are forced (walk_strict or facing) on candidates with the same separating direction s (equal axis and side, so i <= k - spacing and k <= j - spacing along s), giving j - i >= 2 spacing along s. Iterate to a fixed point (implied pairs can chain further, using their implied side). Report the share of undecided pairs implied, and check on every legal drawing that the implied side holds (failures: first 5 in full; a failure means the rule is unsound).
3. A table per fixture and 197: pairs, decided (walk_strict ∪ facing), implied (rule 2), still undecided, and the empirical shares from 1 for the still-undecided.

## 3. Output
The table and a three-line reading in `docs/junction-tool/wip/probes/slots/sides-forced.md` (section sides-5); commit `Doc: Junction: undecided keep-apart pairs sized (sides-5)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. `tools/junction/jt fast build '[planardomain]' 5` must pass.

## 4. Report (short)
References read; commit; the table; rule 2's failures (should be 0); the reading; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
