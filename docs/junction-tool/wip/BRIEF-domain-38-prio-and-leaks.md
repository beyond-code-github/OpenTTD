# BRIEF domain-38: the prio length as a behaviour (known failure) and leaks hidden by the interface

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 60 minutes wall. Test code only. Never run git stash. Do not touch branch wip-defaults.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- Read first: DECISIONS D384, D421, D436; `PlanarRuleFields::coverage` (planar_structure.h ~51: "the exact prio coverage: 2 prio_tiles + 4 (D384)"); in the test file: the seam (`RowsJudge`, `DrawingProgramJudge`, `RULE_NAMES`), `P1Fixture` and its tests, `RunEncoding`'s classing and its off-interface counterfactual, `Tally`, `CLASS_NAMES`.

## 2. Prio length (D384) as a behaviour
D384: "A prio row is exactly its coverage long along its path ... the pair (lane and row) runs exactly the coverage between the row's split and merge". The drawing program has no such row yet.
- `RULE_NAMES` gains nothing now (no tag exists); the behaviour test names the suite rule `prio length`.
- Test A (normal): "planar domain: a prio row exactly its coverage long is accepted (D384)": P1 with the row's lane stretch (row split to row merge along A1#3, which the row A1#4 follows as an offset curve, D421) set so the travel from the row's split node to its merge node equals `rules.coverage` half tiles (state the coverage value and how the travel is measured: along the lane A1#3's points between the two row nodes). Judge and domain accept. If the travel cannot be made exactly `coverage` with A1#3's and the row's lengths in P1 while staying connected and on the grid, stop and report the reachable values.
- Test B (known failure, `[!shouldfail]`): "planar domain: a prio row longer than its coverage is refused (D384)": the same with the travel `coverage + 2`: the domain accepts (it has no prio semantics), and the test asserts the judge refuses with `prio length` among its rules. Comment: names D384 and that the drawing program has no coverage row (STATUS); the test flips to unexpected pass when an implementation adds it.

## 3. Leaks hidden by the interface (report only)
In `RunEncoding`: for a polyline the domain REFUSES and the full judge refuses, also run the counterfactual `judge(f, cv, false)`; if it ACCEPTS, the non-interface rows let a domain-illegal geometry through that only port/box rows stop. Count it in an eighth class "loose off interface" (`counts`/`geoms` grow to 8, `CLASS_NAMES` too, printed per configuration with the first example and the domain's break rule, as too loose prints). It is NOT asserted by the exactness tests (report only). Run all eight fixtures' hidden reports once in Release with the default rules and report the counts and every group's first example.

## 4. Gates and commit
- `tools/junction/jt fast build '[planardomain]' 5` (Debug), `tools/junction/jt gate build-rel` once.
- One commit: `Test: Junction: prio length as a behaviour (known failure); leaks hidden by the interface counted (D384, D436)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Overwrite the eight `..._portzero` reports.

## 5. Report (short)
References read; commit; coverage value and the two travels; test B's shouldfail status; per fixture the loose-off-interface count and first examples in full; gates; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
