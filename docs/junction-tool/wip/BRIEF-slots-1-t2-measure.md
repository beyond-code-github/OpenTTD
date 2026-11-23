# BRIEF slots-1 (measurement): two-stage Slots from Shape's variant (D441, D442)

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 75 minutes wall; each solve capped at 15 s; no rerun loops. Measurement only: a hidden test `[.][slotsmeasure]` and a notes file, committed. No production change.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- D441: Slots v2 = `CheckPlanarDrawable` over contracted lines (D432) with the suite's rules. D442: Shape offers S-bends (k alternating offers per place; offers = 3 and D443's lattice rule carry all of V's 197); Slots decides which are used. With offers = 3 + D443 on, flex pairs: T1 123, T2 345, T3 711, 197 1980 (shape-5). One binary per offer would blow out; the question: does a two-stage solve avoid them?
- Stage 1 (decide the S-bends): the uncontracted program (offers as flex pairs that may be 0; continuous where it can be; no cell rows: `pairwise` + `pairwise_all_faces`, no `RefinePlanarShapeCells`; the default rules). Stage 2 (sound solve): the contracted variant built from stage 1's drawing (`final_contract`'s mechanism: unused offers removed, used ones become steps), solved integer with grid, judged by `DrawingProgramJudge(DEFAULT_RULES)` and the domain.
- Read first: DECISIONS D427, D432, D441, D442, D443; `PlanarDrawPipelineOptions::final_contract` and its implementation (planar_draw.cpp, how it builds the contracted variant from a drawing); shape-4's test (no cell rows) and shape-5's measurement test (how offers / D443 are passed); the seam (`DrawingProgramJudge`, `ContractedVariant`, `PlanarDomainInputOf`).

## 2. Per hub: T1 (`PlanarTText(1)`), T2, T3, and 197 (`tools/junction/corpus/out/psg197` as shape-5 loads it)
1. Shape's variant, flex placed with offers = 3, D443 on (and once with offers = 1, D443 off, as the baseline).
2. Stage 1: status, ms, rows/columns, integer/binary counts; offers used (> 0).
3. Stage 2: the contracted variant from stage 1 (reuse `final_contract`'s code path or its function; if it is only reachable inside `DrawPlanarPipeline`, call the pipeline with only the final stage and say how); status, ms, size; if Optimal: the judge's verdict on its contracted drawing and the domain's on its points (accept/refuse, rules/breaks).
4. A table: hub x (offers, D443) x stage: status, ms, size, offers used, judge, domain.
If stage 1 or 2 times out at 15 s, record it and move on.

## 3. Output
`docs/junction-tool/wip/probes/slots/two-stage.md` (the table, and three lines of reading: does stage 1 pick few offers; is stage 2 feasible and legal; where does time go). Commit `Doc: Junction: two-stage Slots measured on T1, T2, T3 and 197 (D441, D442)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 4. Report (short)
References read; commit; the table; the reading; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
