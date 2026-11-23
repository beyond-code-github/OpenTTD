# BRIEF lazy-1 (trial): lazy keep-apart rows on T1 and T2 (D444)

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 90 minutes wall; each solve capped at 15 s; at most 12 rounds per run; no rerun loops beyond that. One production option (off by default) plus a hidden test; the wide gate once.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- D444: keep-apart rows lazily: solve with none, check, add rows only for pairs that came too close, repeat. sides-1..5 and cells-1/2 (`docs/junction-tool/wip/probes/slots/sides-forced.md`, `cells-measure.md`) showed most pairs' separating axis cannot be fixed upfront; slots-1 (`two-stage.md`) showed writing all pairs' binaries times out even on T1.
- Read first: DECISIONS D381, D432, D441, D442, D444; planar_draw.h `skip_pairs` (~192), `keep_apart_out`, `PlanarKeepApartPair`, `PlanarKeepApartHolds`/`PlanarKeepApartEnd`; the slots-1 test (`[.][slotsmeasure]`: how Shape's variant is built with offers and D443, the stage-2 contraction code it copied from `final_contract`).

## 2. Changes
1. `PlanarDrawOptions`: `std::vector<std::string> only_pairs;` and `bool only_pairs_on = false;` doc: "D444 trial: in pairwise mode, when only_pairs_on, only pairs whose tag is in only_pairs get separation rows (an empty list writes none); every pair is still listed in keep_apart_out." Implemented next to `skip_pairs` in the pair loop. Off by default: no behaviour change otherwise.
2. Hidden test `[.][lazymeasure]` (planar_domain_encoding.cpp), per hub T1 and T2 (`PlanarTText(1)`, `(2)`), flex offers 1 with D443 off (today's defaults) — and, if time remains, offers 3 with D443 on:
   - Stage A (uncontracted, as slots-1's stage 1 but integer with grid on, inserts may be 0): round 0 with `only_pairs_on = true`, empty list; after each Optimal round, collect the pairs (from `keep_apart_out`) whose every candidate fails on the round's points (`PlanarKeepApartHolds` false) and add their tags; re-solve; stop when no pair fails (converged), a round is Infeasible or Error, or 12 rounds.
   - Stage B (contracted from stage A's final drawing, as slots-1's stage 2): the same lazy loop, starting from stage A's final pair list (mapped to stage B's tags if they differ; if they cannot be mapped, start empty and say so).
   - Per round: status, ms, pairs with rows, binaries. Per run: rounds, final pairs with rows vs all pairs, total ms; on convergence of stage B: the judge's verdict (`DrawingProgramJudge(DEFAULT_RULES)`) on its contracted drawing and the domain's on its points.

## 3. Output
`docs/junction-tool/wip/probes/slots/lazy.md`: the per-round tables and a three-line reading (does it converge; how many pairs end up with rows vs all; is the result legal). Commit `Add: Junction: lazy keep-apart rows as a trial option, measured on T1 and T2 (D444)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 4. Gates
`tools/junction/jt fast build '[planardomain]' 5`, `[planardraw]` 30 (Debug), `tools/junction/jt gate build-rel` once (intermittent parallel-load crash: report, pass alone).

## 5. Report (short)
References read; commit; the tables; the reading; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
