# BRIEF domain-13: end runs count from the crosser's footprint

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 60 minutes wall. No rerun loops. Never run git stash. Do not touch branch wip-defaults.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- X2 (`docs/junction-tool/wip/probes/domain/encoding-x2-span_lead_endnode_crosspiece.txt`) finds 63 too-loose polylines (`[connection]`). First example: B1 crosses A1 diagonally at (16, 5), an edge midpoint, so its pieces occupy A1's tiles 7 and 8; the tunnel needs portal and ramp at tiles 9 and 10, but A1 ends at the box edge at tile 9. The `end` rows read the crossing as a point: `len - param >= 2 * least_run` holds at exactly 4. A crosser on a diagonal piece reaches one half tile further along the separator on each side of its crossing point.
- Rule: the end run counts from the crosser's footprint. With f = 1 when the crosser's crossing piece has an odd heading (diagonal), else 0: the first crossing's `end` row becomes `param >= 2 * least_run + f_first`, the last crossing's becomes `len - param >= 2 * least_run + f_last`; `end_node`'s rows add the same f of the crossing they measure to.

## 2. Changes
- planar_draw.cpp, the Spans block (~930-990): compute f for the structure's first and last crossing from `crosser_heading` (already collected); apply as above to the `end` rows and the `end <s> node` rows. Same tags. Always on (it corrects the `end` family, which has no option); the `end_node` part follows `end_node`.
- `src/junction/planar/CLAUDE.md`: in the drawing program's Spans description (end runs), add "counted from the crosser's footprint: one half tile more for a crosser on a diagonal piece".

## 3. Tests
- Fast (harness file): "planar draw: end runs count a diagonal crosser's footprint": X2, the first too-loose example's witness (copy its piece lengths from the report): the program check breaks `end 0` after the change, held before. Red: record before.

## 4. Gates and output
- `tools/junction/jt fast build '[planardomain]' 5` and `tools/junction/jt fast build '[planar]' 15` (report times); `tools/junction/jt gate build-rel`. A previously passing gate test that now fails: stop and report (the `end` family now asks more of drawings: T1/T2 tests may move).
- Release: X1 `span+lead+endnode+crosspiece` (must stay exact) and X2 with the same four sets as domain-12 (overwrite), timeout 300.
- One commit: `Fix: Junction: structure end runs count from the crosser's footprint (D428)`, Co-Authored-By Claude Sonnet 5.5.

## 5. Report (short)
Commit; red evidence; X1 totals; X2 totals per set with group keys, counts, first examples; suites with times; stops; state. When you have reported, stop.
