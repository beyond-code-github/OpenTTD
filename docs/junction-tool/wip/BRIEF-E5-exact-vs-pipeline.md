# BRIEF: E5 T3, the exact drawing's sides against the pipeline's (read-out only)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: about 45 minutes in total. Each solve has a 15 s cap. Run the hidden test once. No rerun loops, no tuning.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (branch wt-e1), head HEAD_HASH, clean. Release build `/home/pete/OpenTTD/.claude/ng1/build-rel`.
- Context: on T3 the exact solve of faces {75, 29, 66} (every keep-apart pair binary, inserts free, `coord_max` 256) is Optimal, while the pipeline's last LP round leaves keep-apart slack 13 with sides read from drawings (D418). This step lists, per wall pair of those faces, which candidate sides the exact drawing satisfies, which candidate the pipeline picked, and the face-walk turn sums, so the orchestrator can see which picks were wrong and what kind of pairs they are. Card: `/home/pete/OpenTTD/docs/junction-tool/wip/CARD-E5-bundle-ends.md` (last two sections).

## 2. Read first (once each)
- The hidden test "planar draw: crowding certificates on T3 and LL2" (exact solve options, `CertWallText`) and "planar draw: LL2 face 33 infeasible core" (candidate pair enumeration) in `src/tests/junction/planar/planar_draw.cpp`.
- `src/junction/planar/planar_draw.h`: `margin_tags`, `sep_margins` (text format `"c<k> s(x,y) spacing <n> from i|j margin <m>"` joined by `"; "`), `sep_sides`, `PlanarDrawPipelineResult::last_input`, `last_round`.

## 3. Change: one new hidden test "planar draw: T3 exact sides against the pipeline's", tag `[.][planarcert]`
No production code changes.
1. `c = ShapeFirst(PlanarTText(3), 1)`. Run `DrawPlanarPipeline(c.v, o)` with defaults and `o.final_seconds = 5`; keep `r.last_input` and `r.last_round`.
2. Exact solve: the certificate test's options with `only_faces = {75, 29, 66}`, `coord_max = 256`. It must be Optimal; if not, WARN and stop the test.
3. Candidates: every (face, i, j) with i < j over each face's walls, not adjacent (`j == i + 1` or `i == 0 && j == n - 1`), both walls tracks (not box), as tags `sep f i j`.
4. Two margin read-outs with the pipeline's last-round options (copy the option list of the commit-stage round from `DrawPlanarPipeline`, or the closest the test can build: pairwise, pairwise_all_faces, sep_extremal, corridor, elastic with families {"sep", "curve"}), `margin_tags` = all candidate tags:
   - (A) `sides_from = &r.last_input` (the reference the last round picked from): for each tag, the pipeline's pick is the candidate with the largest margin (first on a tie), as the code picks.
   - (E) `sides_from = &exact.points`: each candidate's margin on the exact drawing.
   Tags that do not appear in `sep_margins` were skipped by the code (adjacent, span, zero contraction); list them once as skipped.
5. Turn sums: for a pair (i, j) in a face with walls w, `turn(i, j)` = sum over k from i to j - 1 (indices mod n, walking forward) of `norm(w[k + 1].heading - w[k].heading)`, with `norm(x) = ((x % 8) + 12) % 8 - 4` (the same normalisation as the pairwise block), in eighths; `turn(j, i)` likewise from j round to i.
6. WARN one line per non-skipped candidate:
   `pair <tag>: <wall i text> | <wall j text>; turn i->j <t1> j->i <t2>; pipeline pick c<k> (margin on exact <m>); exact holds <list of c<k> with margin >= 0 on the exact drawing>; stretched <amount or 0 from r.last_round.stretched>`
7. Summary WARNs: count of pairs; count where the pipeline's pick has margin < 0 on the exact drawing ("contradicted"); the contradicted tags; and the (t1, t2) histogram over contradicted pairs and over all pairs.

## 4. Do not
- No production code change, no default change, no interpretation.

## 5. Stop conditions
- The exact solve is not Optimal.
- The pipeline's last-round options cannot be reproduced in the test without production changes: use the closest and state the difference in the report (no stop).
- A gate fails for a reason you did not cause and cannot isolate in one rerun.

## 6. Gates and measurement
- `cd /home/pete/OpenTTD/.claude/ng1 && tools/junction/jt fast build-rel '[planardraw]' 5` (the new test is hidden; this checks the build).
- `build-rel/openttd_test "planar draw: T3 exact sides against the pipeline's" > build-rel/exactsides.log 2>&1`; copy to `docs/junction-tool/wip/probes/e5land/exactsides-<short head hash before your commit>.log`, same commit.
- One commit: `Add: Junction: T3 exact sides against the pipeline's, a read-out (E5)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. No merge or push.

## 7. Report (short, fixed format)
- Commits: hash and subject; files changed.
- Suite result.
- The summary lines verbatim; every contradicted pair's line verbatim; the stretched pairs' lines verbatim (join Catch's 80-column wraps first). The full per-pair list stays in the log.
- Stops and anything left undone; any difference from the last-round options.
- State: `git status --porcelain` and `git log -1 --oneline`.

When you have reported, stop.
