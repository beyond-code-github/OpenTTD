# BRIEF stageb-miss: T1's stage B converges but the judge and the domain refuse it (pin + diagnosis)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 60 minutes. Stage B runs from the committed stage A fixture (no stage A solve). This step pins and diagnoses; it does NOT fix.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng2`, branch `wt-c2`, head `<HEAD>`. Build Release `build-rel`. Load limit `JT_JOBS=4`, cmake `-j4`. Never touch ng1.
- Evidence (b46e0a69c1's probe run, T1 with `PLANAR_GRID_COND=1 PLANAR_STAGEA_NODEGAPS=1`): stage B converged in 2 rounds (8 of 198 pairs, 54 ms), polish Optimal 150 -> 134, then the judge refuses `[keep apart]` and the domain has 1 break `[connection]` (tracks meet with no node). Stage B's lazy loop thought the drawing clean; two independent checks disagree.
- Fixture: `tools/junction/topologies/t1-stagea-rules.drawing.txt` (stage A, pinned green by "... stage A with grid parity and node gaps (T1)").

## 2. Read first (once each, offset/limit)
- `src/tests/junction/planar/planar_domain_encoding.cpp`: the `[slotsstep2]` probe from the contraction to the judge/domain lines (stage B lazy rounds, which pairs it adds and by what detector, the polish, the judge `DrawingProgramJudge`, the domain call); `StageBOptions`; the fixture-pin helper `CheckContractedDrawingSatisfiesStageB`.
- `src/junction/planar/CLAUDE.md`: "Keep-apart read-out" and "Domain" bullets.

## 3. Changes
1. A hidden test `[.][stagebmiss]` "stage B from the pinned T1 stage A drawing: lazy rounds, polish, judge, domain" that runs the probe's stage B + polish + judge + domain from the fixture (reuse the probe's code by moving the stage B..domain part into a file-local function both call; probe output unchanged), deterministic: run it twice and confirm identical lines.
2. The pin (`[junction][contraction]` + `[!shouldfail]`): "T1's stage B drawing from the pinned stage A is accepted by the judge and the domain". Save stage B's final (polished) points as `tools/junction/topologies/t1-stageb-rules.drawing.txt` (same format as the stage A fixtures: `track <name> <points...>` or lengths, your choice, stated in the file header comment line) and assert on that fixture with no solve: judge and domain accept. Red is expected (the refusal).
3. Diagnosis `docs/junction-tool/wip/probes/slots/stageb-miss.md` (at most 60 lines): the domain break (tiles, the two tracks and pieces, whether they belong to one face pair), the judge's keep-apart refusal (the pair, its rows as the judge writes them, margin on the drawing), and for that pair: did stage B's lazy detector see it (and if not, why: skip reason, not in `only_pairs_on`, a different pairing rule than the judge), or did it add rows that the polish then broke (polish has fewer rows than stage B?). One sentence: which component is wrong, by the contract each states.

## 4. Do not
- No fix. No change to stage A, stage B rows, options or defaults. Source edits with Edit/Write only.

## 5. Gates and commit
- `JT_JOBS=4 tools/junction/jt fast build-rel '[contraction]' 5`, `... '[planardomain]'` (unchanged).
- Commit: `Add: Junction: pin, T1's stage B drawing is refused by the judge and the domain (red)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## 6. Report
References read; commit; red text; the diagnosis in at most 10 lines; suite results; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
