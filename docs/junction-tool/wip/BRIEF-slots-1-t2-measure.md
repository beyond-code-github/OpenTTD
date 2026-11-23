# BRIEF slots-1 (measurement): T2 straight from Shape through today's program (D441)

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 60 minutes wall; each solve capped at 15 s; no rerun loops. Measurement only: no production change. A hidden test may be added (tag `[.][slotsmeasure]`) and committed with the notes.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- D441: Slots v2 starts as `CheckPlanarDrawable` over contracted lines (D432) with the suite's rules, judged by `RowsJudge` (D436). First milestone: T2 solved from Shape's own variant, no seed, no pins. Open question: contracted rows need to know which S-bends exist (D432 took them from a previous drawing via `final_contract`); with no seed, do they come from Shape (its flex/steps) or must Slots choose them?
- Read first: DECISIONS D395, D402, D427, D432, D436, D441; planar_draw.h `PlanarDrawOptions` (rules on by default since D435), `final_contract`; planar_draw.cpp the per-track flex/step handling (~520-640); in planar_draw.cpp tests, `ShapeFirst(PlanarTText(2), 1)` (how T2's Shape variant is made); in the domain-suite test file, `ContractedVariant`, `DrawingProgramJudge`, `PlanarDomainInputOf`.

## 2. Solves (T2's Shape variant; each 15 s cap, integer, grid on, tidy on, `corridor = true`; record status, ms, rows, columns, binaries)
1. **Flex as today (uncontracted):** Shape's variant with its flex groups (inserts may be 0), `clearance = pairwise = pairwise_all_faces = true`.
2. **All flex removed:** the variant with every flex pair erased (as `final_contract` erases an unused pair: the insert and its trailing flank removed, one straight piece left), so the only S-bends are Shape's `steps` (if T2's variant has none, say so).
3. **Every flex pair kept as a step:** every flex insert becomes a step (a fixed bend pair, positive length), as `final_contract` does for a committed insert.
4. Each Optimal drawing from 1-3: write it as a polyline, contract it (`ContractedVariant`), judge it with `DrawingProgramJudge(DEFAULT_RULES)` and the domain (`CheckPlanarDomain(PlanarDomainInputOf(...))`): accept/refuse and rules/breaks.
5. Same three solves on T1 (`PlanarTText(1)`) for comparison.
If a solve is Infeasible, give its conflict tags (the first 20); if it times out, say so (no retry with more time).

## 3. Output
`docs/junction-tool/wip/probes/slots/t2-measure.md`: a table (hub, variant form, status, ms, size, judge verdict, domain verdict) and a few lines of reading: which form gives a legal drawing fastest, and what that says about where S-bends must come from. Commit: `Doc: Junction: T2 and T1 straight from Shape through the drawing program (D441)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 4. Report (short)
References read; commit; the table; the reading; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
