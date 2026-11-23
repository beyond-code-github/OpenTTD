# BRIEF domain-42: a tunnel head may sit above another tunnel's middle (domain gap from V's 197)

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 60 minutes wall. A domain change; the wide gate once. Never run git stash. Do not touch branch wip-defaults.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- Read first: `docs/junction-tool/wip/probes/domain/triage-197-judge.md` (the NEW domain breaks section); `src/junction/planar/planar_domain.cpp` (the `head` rule and the missing-crossing rule, as domain-40 left them, including its tunnel-under-tunnel acceptance); `src/tests/junction/junction_game.cpp` pin 8 (domain-40's game pin of tunnel under tunnel) and the hand cases in `src/tests/junction/planar/planar_domain.cpp`.
- domain-41: with portal-only end runs, V's 197 shows 14 domain breaks (head 7, missing crossing 7) in 7 pairs with one cause: a tunnel's HEAD tile directly above the MIDDLE of a perpendicular tunnel (L11/L47 head at game (92,171), L12/L47 (92,172), L11/L57 (95,169), L12/L57 (95,170), L45/L48 (93,166), L45/L59 (96,166), L46/L59 (96,167)). All nine tiles equal V's, so the game allows it.

## 2. Changes
1. The domain: a tunnel head (an end piece of a TUNNEL structure) may share its tile with the middle of another TUNNEL structure crossing it (by depth: the lower tunnel passes beneath the portal); that listed crossing is realised. Bridge heads and bridge decks are unchanged. Keep the change to the two rules that refused it.
2. Hand cases: domain case "a tunnel head above another tunnel's middle" (legal), and its counterpart "a bridge head on another bridge's deck tile" (still refused). Pin the legal case in the game (`[planardomaingame]`, as pin 8: the lower tunnel under the upper's portal tile, levels chosen so the game builds both; and show the game refuses it with equal levels so the pin is not vacuous). If the game refuses the legal case at every level the harness can make, STOP and report (then V's save holds something the harness cannot build; give the tiles).
3. Rerun the 197 test in Release: the domain's breaks should be the 6 triage artefacts only; report them.

## 3. Gates and commit
- `tools/junction/jt fast build '[planardomain]' 5`, `[planardomaingame]`, `tools/junction/jt gate build-rel` once; the eight exactness tests 0/0.
- One commit: `Fix: Junction: a tunnel head may sit above another tunnel's middle (D390, D393)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`; planar CLAUDE.md domain bullet updated (D225); the regenerated answerkey-197.txt.

## 4. Report (short)
References read; commit; hand cases and the game pin (levels, and the equal-level refusal); 197 domain breaks after; gates; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
