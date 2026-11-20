# BRIEF E1d-1b: straight legs of PSG 197 (fixes E1d-1's rule)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Repo /home/pete/OpenTTD, branch feature/junction-tool, head HEAD_HASH (check with `git log -1`). Main checkout, data only: no build, no tests, one Python process at a time.
- E1d-1 (brief docs/junction-tool/wip/BRIEF-E1d1-psg197-structure-file.md, commit 599e74c5f1) wrote tools/junction/topologies/psg197.structure.txt; its tunnel, group and copy statements are accepted and must not change. Its straight-leg rule failed: the trunk's walk and each leg's walk share the node tile's piece, so both legs matched (60 of 62 nodes undetermined); one-piece walks got arbitrary headings (CD2). Read E1d-1's brief section "Output 1" item 4 and docs/junction-tool/wip/probes/e1/structure.py (its straight-leg code and the walk code it copied from order.py), and structure-out.txt's per-node lines.
- Budget: 30 minutes.

## The rule (replaces item 4's comparison; everything else in item 4 stands, including the `none` and `undetermined` outputs)
- Trunk heading h: the heading of the trunk's last segment (split) or first segment (merge) that does not lie on the node tile. A trunk with no such segment (a port line whose walk is only the node tile, e.g. CD2) uses its port's travel heading from psg197.txt's `port` statement.
- Leg heading: for each leg, its first segment (split) or last segment (merge) that is not a segment of the trunk's walk (compare segments by tile and piece). This is the leg's own piece on the node tile, or the first piece after it when the leg's own node-tile piece is in the trunk's walk.
- Exactly one leg with heading h: `straight <trunk>: <that leg>`. Neither: `straight <trunk>: none`. Both, or a walk missing: `# straight <trunk>: undetermined (<reason>)`.
- The 3-way split at 92,158 (gaps §12): A2m has no tiles. Apply the rule to trunk A2 with the three legs A2b, A2c, A2d. If A2d is the one straight leg: `straight A2: A2d` and `straight A2m: none`. If A2b or A2c is: `straight A2: A2m` and `straight A2m: <it>`. Otherwise both nodes undetermined.

## Changes
- structure.py: replace the straight-leg comparison by the rule above; regenerate psg197.structure.txt and structure-out.txt. Per node, structure-out.txt prints the trunk heading and its source (segment or port), each leg's compared segment (tile, piece, heading) and the result.
- Check (a breach is a stop): the tunnel, group and copy lines of psg197.structure.txt are byte-identical to 599e74c5f1's (`git diff` shows only `straight` lines changed). Report counts of straight, none, undetermined (each undetermined listed with its reason). More than 6 undetermined is a stop: report them, commit nothing.

## Commit
`Doc: Junction E1d-1b: PSG 197 straight legs by the leg's own piece (D389)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`; only structure.py, structure-out.txt, psg197.structure.txt. Do not push.

## Report (short): references read; commit; counts; undetermined list; the check; `git status --porcelain`, `git log -1 --oneline`. When you have reported, stop.
