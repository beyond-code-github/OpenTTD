# BRIEF domain-14b: lead rows on contracted lines (S-bends as steps)

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 60 minutes wall. Never run git stash. Do not touch branch wip-defaults.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- D432: Slots writes rows over contracted lines. The suite now judges each polyline on its contracted variant (`ContractedVariant` in the harness), which has empty `flex` and `steps`. `PlanarNodeLeadRows` (D429/D430, planar_draw.cpp) builds rows only from `flex` middles, so on contracted variants it returns none, and X1 shows 45 too-loose polylines (legs overlapping at a split; e.g. S1 B1 contracted `[1x2, 0x10]`: B1 leaves the node on its NE insert, over D1).
- In a contracted line a used S-bend is a real bend pair: an insert piece (heading h +- 1) whose flanks (heading h) may be absent next to a node. Rule (D430 in contracted form): at a leg's node end, an S-bend that steps toward the sibling leg has a node-side travel of at least one game piece before its insert; one that steps away is free.

## 2. Changes
- Harness `ContractedVariant`: record each contracted piece that came from a flex middle of the representation (an S-bend insert) in `track.steps` (its index in the contracted pieces). Pieces keep their lifted headings.
- `PlanarNodeLeadRows` (planar_draw.cpp/.h): the leading walk takes the S-bend middles from `flex` when `flex` is not empty, else from `steps`. For a middle m in the leading walk (ascending from the start; all pieces before it are flanks of heading h0 or S-bend inserts):
  - h0 is the leg's node-rule heading: when piece 0 is itself an insert (m == 0), h0 is the heading of the pieces before it in the original, which the contracted variant does not hold: take it as `pieces[m + 1]` when that piece exists and is not an insert (an S-bend's far flank keeps h0), else no row.
  - Side d = sign(pieces[m] - h0); toward when `sib == 0 || d == sib` (sibling side as domain-4).
  - Row as now: `64 * T - I >= 0` with T = STEP[h0] . (point(t, m) - point(t, 0)) and I = STEP[pieces[m]] . (point(t, m + 1) - point(t, m)). When m == 0, T is 0 and the row fails whenever the insert has length (it always has, contracted): that is the rejection.
  - End rows mirrored. Flex behaviour (non-empty `flex`) is unchanged.
- Doc comment and the planar CLAUDE.md lead bullet: "on contracted lines (D432) the S-bends are `steps`; a toward step at the node with no node-side flank is rejected".

## 3. Tests (fast)
- "planar draw: node lead rows on contracted lines": X1 S1's first too-loose example (from `encoding-x1-span_lead_endnode_crosspiece.txt`): its contracted variant gives `lead B1 start 0` failing; X1's base contracted: every lead row holds; the D430 outward case (A1 leaving the split on its SE insert, as in X1's domain-4 example) contracted: no lead row fails. Red: today's function returns no rows on contracted variants.

## 4. Gates and output
- `tools/junction/jt fast build '[planardomain]' 5`; `tools/junction/jt gate build-rel` (production change; one rerun if only a known flaky case fails, report it).
- Release once each: X1 and X2, `span+lead+endnode+crosspiece` (overwrite).
- One commit: `Change: Junction: node lead rows read contracted S-bends (D430, D432)`, Co-Authored-By Claude Sonnet 5.5.

## 5. Report (short)
Commit; red evidence; X1 and X2 classes with group keys, counts and first examples; the lead-row count over judged variants; suites with times; state. When you have reported, stop.
