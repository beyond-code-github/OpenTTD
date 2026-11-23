# BRIEF domain-46 (diagnostic): why TS2 (three lanes, a two-crosser tunnel) has no drawing

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 60 minutes wall; each solve capped at 15 s; no rerun loops. Diagnosis: fix no rule. A hidden test may be added; commit it with the notes. Never run git stash.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- TS2 (domain-43; texts in the commit body of the TT1 commit and in scratchpad `domain43-wip.diff`): L1 widened to three lanes, `tunnel T1 B1: A2, A3`, `tunnel T2 B2: A3`, straights `AB<k>: B<k>`. Shape accepts it (headings in the domain-43 report); the drawing program's base (`CheckPlanarDrawable`, the DrawnFixture options, integer, grid, tidy, `least_extra = 2`) is Infeasible; the conflict list is not minimal (persisting: `cross 0`, `cross 2`, `port E gap B2/B3`, `port W gap AB2/AB3`, `node 0..2`, pieces of AB1, AB3, A2, A3, B1, B3).
- Read first: the DrawnFixture constructor; `PlanarDrawOptions::skip_rows` (deletion filter by tag); the TS2 texts.

## 2. Steps (each one solve, 15 s cap)
1. Same options with `least_extra = 0`: status.
2. Without grid (`grid = false`): status. Without `end_node`, without `cross_piece`, without `node_lead` (one at a time): status each.
3. If still Infeasible with everything optional off: a minimal conflict by deletion filtering over row FAMILIES (tag up to the first space) with `skip_rows` (prefix entries end in a space): drop one family at a time; keep it dropped if still infeasible; report the families whose removal makes it feasible, then within the last needed family the individual tags (same filter), at most 40 solves in total.
4. Say in plain words what the minimal conflict means geometrically (e.g. "A3 must cross B2 and B1 at the same angle under both tunnels, and the E port's exact lane spacing leaves its S-bend no room").
5. Also: does V-style relaxing of the W/E port spacing (`@` positions one tile wider apart per lane) make it feasible? One solve.

## 3. Output
`docs/junction-tool/wip/probes/domain/ts2-diag.md` (each solve's options and status, the minimal conflict, the plain reading); commit `Doc: Junction: TS2 base infeasibility diagnosed`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 4. Report (short)
References read; commit; the status table; the minimal conflict and its reading; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
