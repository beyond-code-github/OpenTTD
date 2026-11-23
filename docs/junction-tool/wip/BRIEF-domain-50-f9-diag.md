# BRIEF domain-50 (diagnostic): F9, keep-apart too strict next to a portal-only tunnel end

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 45 minutes wall. Diagnosis only: no rule change. A temporary probe may be added and must be removed; commit only the notes file. Never run git stash.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- D437 (ee19dcd355) made a tunnel's end run its portal (1 piece). X2 (a 45-degree crosser B1 bending at its crossing with A1's tunnel, D414) now has 23 too-strict polylines (F9; `git show ee19dcd355` body has the first examples: `B1:0*2,1*3,2*2,1*1,0*4` refused by `keep apart` + `port`; the counterfactual without port/box rows still refuses by `keep apart`). The domain accepts them.
- Read first: the keep-apart pair loop in planar_draw.cpp (~1451-1620: `adjacent`, `span`, `zero track`, `zero contraction` skips and the separation candidates); the X2 fixture; `DrawingProgramJudge`; D376, D381, D414, D437.

## 2. Find
1. For the first example of each group: list the keep-apart pairs that fail (tag, the two walls: track, piece, heading, endpoints), with their candidates' slacks.
2. For each failing pair, draw (in text) the tiles around it from the domain's raster: where the separator's portal and end-run piece are, where the crosser's pieces are, the gap between them.
3. Say why the domain accepts (which tile rule allows that closeness, e.g. the crosser's diagonal shares the tile edge outside the portal) and which keep-apart skip covered the same situation under end run 2 (e.g. the `span` skip reached the crosser's neighbouring piece because the 2-piece end run kept the bend one piece further from the crossing).
4. Propose the general rule (one sentence, in terms of structure, end run and crossing, not of X2) that makes the pair skip exactly when the game allows the closeness, and list which other fixtures' pairs it would also skip (count, from their read-outs) — the risk of making something too loose.

## 3. Output
`docs/junction-tool/wip/probes/domain/f9-diag.md`; commit `Doc: Junction: F9 diagnosed (keep-apart next to a portal-only end)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 4. Report (short)
References read; commit; the failing pairs; the reading; the proposed rule and its reach; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
