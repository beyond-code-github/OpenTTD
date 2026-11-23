# BRIEF lazy-4: lazy pairs with their sides read from the drawing that broke them; and the offers/copy conflict

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 90 minutes wall; each solve capped at 15 s; at most 12 rounds per stage. One production option (off by default); the wide gate once.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- lazy-3 (`docs/junction-tool/wip/probes/slots/lazy.md`): T2 offers 1: stage A round 0 (no keep-apart rows) Optimal in 2.4 s with 51 pairs too close; round 1 with those 51 pairs (204 binaries) hits 15 s. T2 offers 3 + D443: stage A Infeasible with no rows: `cross 12` (D1 x E2), `cross 19` (D1 x prio:E2), `corridor`, `piece`.
- Read first: lazy.md; planar_draw.h `only_pairs`/`only_pairs_on`, `sides_from`, `forced_sides`; the pair loop (how a pair's candidates become rows with binaries, and how `sides_from` fixes a pair to its best candidate as one plain row); `PlanarShapeTrack`'s beside handling in `PlacePlanarShapeFlex` (the rule that a beside track's parent chain gets no flex) and the shape-5 offers/D443 code.

## 2. Part A: fixed sides for lazily added pairs
1. `PlanarDrawOptions`: `std::vector<std::pair<std::string, int>> fixed_pairs;` doc: "D444 trial: these pair tags take the given candidate (axis*2+side) as one plain row, no binary; with only_pairs_on, a pair listed here gets rows even if not in only_pairs." (If `forced_sides` already does exactly this without `sides_from`, reuse it instead and say so; if it requires `sides_from`, add `fixed_pairs`.)
2. The lazy loop (both stages): a pair that fails on round k's points is added with its candidate FIXED to the one with the largest margin on round k's points (closest to holding). If a round with fixed pairs is Infeasible, move the pairs named in its conflict (`result.conflict` tags starting `sep`) from fixed to free (binaries, as in lazy-1) and re-solve; record how many were freed.
3. T2 offers 1/D443 off: stage A and B; per round status, ms, pairs fixed, pairs free, binaries; on convergence the judge's and the domain's verdicts. T1 both configs as a regression (must still converge and be accepted).

## 3. Part B: the offers/copy conflict (diagnose only)
T2 offers 3 + D443: print E2's and prio:E2's pieces and flex, D1's crossings with both (which pieces), and whether E2 (prio:E2's parent) got flex pairs it should not have (the beside-parent rule) with offers 3 or with D443. Say which of the two parameters causes the conflict (run offers 3/D443 off and offers 1/D443 on, stage A round 0 only) and the rule that would prevent it. No fix.

## 4. Gates and commit
- `tools/junction/jt fast build '[planardomain]' 5`, `[planardraw]` 30, `tools/junction/jt gate build-rel` once (intermittent crashes: rerun alone, report).
- One commit: `Add: Junction: fixed sides for lazily added keep-apart pairs, as a trial option (D444)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`; results appended to lazy.md.

## 5. Report (short)
References read; commit; Part A tables and verdicts; Part B diagnosis; gates; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
