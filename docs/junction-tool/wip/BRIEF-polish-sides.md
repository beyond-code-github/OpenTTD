# BRIEF polish-sides: the polish keeps every keep-apart pair on the side stage B's clean drawing gives

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 45 minutes. No stage A solves (fixtures only).

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng2`, branch `wt-c2`, head `<HEAD>`. Build Release `build-rel`. `JT_JOBS=4`, cmake `-j4`. Never touch ng1.
- Diagnosis `docs/junction-tool/wip/probes/slots/stageb-miss.md`: stage B's lazy loop ends clean; the polish re-solves with `only_pairs = b.list` (8 of 198 pairs), shortening 150 -> 134 moves CD1 and AF1 onto (15,24): 4 unlisted pairs of face 5 break (judge `[keep apart]`, domain `[connection]`).
- Rule (orchestrator, under D445/D448): the polish is drawing-preserving by construction: every keep-apart pair gets rows, each on the candidate side `sides_from` picks on stage B's final (clean) drawing (plain rows, no binaries, `PlanarDrawOptions::sides_from`; CLAUDE.md "Pairwise mode"). Stage B's drawing satisfies every such row, so the polish is feasible and keeps every pair apart.

## 2. Read first
- `src/junction/planar/CLAUDE.md` "Pairwise mode" bullet (`sides_from`, `short_sides`); `planar_draw.h` `sides_from`, `only_pairs`, `only_pairs_on`.
- `planar_domain_encoding.cpp`: `StageBToDomain` (the polish part), the `[stagebmiss]` hidden test, the red pin on `t1-stageb-rules.drawing.txt`.

## 3. Changes
1. In `StageBToDomain`'s polish: no `only_pairs`/`only_pairs_on` restriction; `sides_from` = stage B's final points. Print `short_sides` in the polish line (it must be 0: stage B's drawing is clean; a non-zero count is a stop, report the pairs).
2. **Pin (outer, fast, red first):** `[junction][contraction]` "stage B and the polish from the pinned T1 stage A drawing are accepted by the judge and the domain": `StageBToDomain` from `t1-stagea-rules.drawing.txt`; asserts polish Optimal, judge accept, domain accept. Red before step 1 (record the text), green after. Same case for T2 (`t2-stagea-rules.drawing.txt`) as a second TEST_CASE: if stage B is Infeasible there (the known keep-apart limit), mark it `[!shouldfail]` and report the status line; do not chase.
3. The old red pin on `t1-stageb-rules.drawing.txt` stays (it pins a drawing, not the pipeline); add one line to its comment: superseded by the pipeline pin, kept as the evidence.
4. Probe: unchanged apart from the shared function. Run T1 once with `PLANAR_FLANK=1 PLANAR_NODE_FLANK=1 PLANAR_GRID_COND=1 PLANAR_STAGEA_NODEGAPS=1 JUNCTION_MILP_INCUMBENT=1` (one 15 s stage A) and report polish length (was 150 -> 134), judge and domain.

## 4. Do not
- No change to stage B's lazy loop, stage A, the judge, the domain or defaults. Source edits with Edit/Write only.

## 5. Gates and commit
- `JT_JOBS=4 tools/junction/jt fast build-rel '[contraction]' 5`, `... '[planardomain]'` (unchanged).
- Commit: `Fix: Junction: the polish keeps every keep-apart pair on stage B's side`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## 6. Report
References read; commit; red text; `short_sides`; T1 polish/judge/domain (pin and probe); T2 status; suites; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
