# BRIEF domain-41: 197's answer key with portal-only end runs, and the judge's refusals triaged

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 90 minutes wall. Test code and diagnosis only: fix no rule in the domain, the judge or the drawing program. Never run git stash. Do not touch branch wip-defaults.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge. First add `tools/junction/corpus/__pycache__/` to the worktree's `.gitignore` if a gitignore covers tools/ (else leave it untracked) — do not commit it.
- Read first: `docs/junction-tool/wip/probes/domain/triage-197.md`, `answerkey-197.txt`; `wip/CARD-tunnel-slopes.md`; the 197 hidden test, `triage197.py`; the seam (`DrawingProgramJudge`, `RULE_NAMES`, `RuleOf`) in `planar_domain_encoding.cpp`.
- domain-40 fixed two domain gaps; 55 domain breaks remain on V's 197, most from tunnel end runs: the test gives every tunnel `ends = 2` (portal + ramp on flat ground), but V's heads are often on slopes (portal only), and the export cannot classify every head. For JUDGING V's build, the structure is the portal: the tile outside it is terrain (a dug ramp or lower ground), and wherever V has a ramp V's track there is straight anyway. So the answer key uses `ends = 1` for every tunnel (bridges stay 1). This is the answer key's input only; the planner's end-run rule is the engineer's card.

## 2. Changes
1. The 197 test: every structure's domain `ends = 1`, with a comment giving the reason above (cite triage-197.md and the card). Rerun in Release. Report the remaining domain breaks by rule; classify each against the triage's artefact list (box edge L94/L96, L69/L70 node, L62xL60 diamond) or as NEW (give it in full).
2. `RULE_NAMES`: map the families `deck` -> `deck`, `node` -> `node`, `order` -> `port order`, `span` -> `span` (suite names; they are existing drawing-program rows). Re-run the 197 report so no refusal shows as `other`.
3. The judge's refusals on V's 197 (expected families: end run, keep apart, grid, node gap, deck, node, port order): for EACH refusing row (all of them, there are about 35), read its terms on V's points (the row's tag names the tracks/pieces; evaluate the row's sum against its bound) and V's tiles there (`triage197.py` can show V's pieces on a tile), and class it:
   - PLANNER RULE: the row encodes our rule that V does not follow (e.g. end run 2 vs V's portal-only heads: the card) — name the rule and decision;
   - ARTEFACT: V's points at that row are the rebuild's (one of the triage's artefacts or a new one; name it);
   - TOO STRICT: V's tiles there are legal, the domain accepts them, and the row refuses a geometry the rule's own statement allows — the finding we want; give the tracks, tiles, the row's sum and bound, and the decision the rule comes from.
4. Write the classification into `docs/junction-tool/wip/probes/domain/triage-197-judge.md` (one line per refusing row, counts per class and family, the TOO STRICT ones in full).

## 3. Gates and commit
- `tools/junction/jt fast build '[planardomain]' 5` (Debug).
- One commit: `Doc: Junction: V's 197 judged with portal-only end runs; the judge's refusals triaged (D393, D436)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 4. Report (short)
References read; commit; domain breaks after (by rule, artefact or new); the judge's refusals by class and family; every TOO STRICT in full; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
