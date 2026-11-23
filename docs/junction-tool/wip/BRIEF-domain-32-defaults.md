# BRIEF domain-32: the suite-cleared rules become defaults (D435)

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 90 minutes wall (the wide gate included). Production defaults change: the wide gate runs once, in Release. Never run git stash. Do not delete or move branch wip-defaults (cherry-pick from it only).

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- Read first: `/home/pete/OpenTTD/docs/junction-tool/DECISIONS.md` row **D435** (this step) and D428; `CODINGSTYLE.md`; `docs/junction-tool/TESTS.md` (gate budgets); `src/junction/planar/CLAUDE.md` (its option bullets for node_lead, end_node, cross_piece, final_grid); commit `55c33d1ecc` on branch `wip-defaults` (`git show 55c33d1ecc`): it turns `node_lead`, `end_node`, `cross_piece` on in `PlanarDrawOptions`, adds the same three to `PlanarDrawPipelineOptions` and passes them into every pipeline stage, and updates CLAUDE.md.

## 2. Changes
1. `git cherry-pick 55c33d1ecc` onto wt-e1. Resolve conflicts textually, keeping both sides' additions (the branch has since gained options such as `port_zero`, `least_extra`). If a conflict is not purely textual (two changes to the same option's meaning), abort the cherry-pick and stop. Amend nothing yet.
2. In every doc text the cherry-pick brings (planar_draw.h, CLAUDE.md), replace "(D428: X1 domain suite exact)" with "(D435: the domain suite exact on X1, X2, M1, K1, L1, Y1, P1, BR1)".
3. `PlanarDrawPipelineOptions::final_grid` default true; its doc: `///< On by default (D435): the integer final writes the grid rows (PlanarDrawOptions::grid).` CLAUDE.md: the final_grid bullet (or the pipeline bullet that names it) says it is on by default (D435). `PlanarDrawOptions::grid` itself stays false.
4. The D414 T1 test ("planar draw: a crosser may cross at its piece end (D414)", src/tests/junction/planar/planar_draw.cpp ~1555) is deleted. In its place, in planar_domain_encoding.cpp: fast `[junction][planar][planardomain]` "planar domain: X2's crosser bending at its crossing is accepted under the default rules (D414, D435)": X2's base polyline, judged with the fixture's options EXCEPT that `node_lead`, `end_node`, `cross_piece` are left at their defaults (do not set them), grid on: the rows accept it and the domain accepts it.
5. The domain suite fixtures that set these three flags explicitly keep doing so (their `+lead`/`+endnode`/`+crosspiece` suffixes must still be able to turn them OFF): check `RunEncoding` sets each to the suffix's value (true or false), not only to true; if it only ever sets true, make it assign the parsed bool. Report what you found.
6. One commit for all of it: `Change: Junction: the suite-cleared rules on by default (D435)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. The commit also carries the CLAUDE.md update (D225).

## 3. Gates (in this order; output to files)
- Debug: `tools/junction/jt fast build '[planardomain]' 5`, `tools/junction/jt fast build '[planardraw]' 30`.
- Release once: `tools/junction/jt gate build-rel`. On any failure: do not investigate or rerun; commit the work as `WIP: Junction: defaults on (D435), gate failing` instead of the commit in item 6, and report each failing test's name, its first failing assertion and the gate's wall time against its budget.
- Do not rerun a failing gate in a loop.

## 4. Report (short)
References read; commit; conflicts and how resolved; the RunEncoding finding (item 5); the new test's red evidence is not needed (it asserts a property that holds); gates with counts and times; every gate failure in full; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
