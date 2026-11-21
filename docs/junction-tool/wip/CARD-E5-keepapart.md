# CARD E5-KA: keeping tracks apart in free faces where a named landing fails (orchestrator, 2026-11-21)

Implements CARD-E5 program items 3-4 (landing, clearance per cell); follows D405 (fallback A is ruled "only if such dummies make a hub undrawable": Shape's 197 now is, faces 16 and 17, wip/probes/e5land/NOTES.md Round 2).

## Context
- Shape's 197 has 162 faces with dummies (865 dummies). With named landings, 158 faces draw alone, 2 are undecided at 15 s (61, 99) and 2 are infeasible (16, 17). Each infeasible face has one monotone dummy whose landing wall cannot reach the ray sideways.
- Example, face 17: SO2 runs SE (l1), S, SW (l3) back to its port. The north ray from the hook's end hits the SE leg only if l1 >= l3, but the south port order and nodes 51/54/60 force l3 - l1 = 7.
- With diagonal walls, where a ray lands always depends on lengths, so no rule on headings alone can name a landing that works for every set of lengths.

## Options
**A. The drawing program chooses the landing (D405's fallback).** Each dummy keeps Shape's heading. Its candidates are the face's walls where a landing leaves both end corners convex, which can be read from headings. One binary per candidate, exactly one chosen, and big-M landing rows.
- Cells now depend on the choices. Each clearance row of a cell holds only when every dummy bounding that cell made the choice that forms it: `lhs >= spacing - M * sum(1 - b)`. The cells must be listed for every combination of local choices, including the order in which two dummies land on one wall, and any landing on another dummy.
- Keeps D385's cells and H12 as written. The cost is new machinery: cells are listed per choice, and only clearance (already the expensive family) gets the binaries. Its row count can't be estimated without building the enumeration.

**B. Pairwise separation in every face Shape refines (Nöllenburg & Wolff 2011, written before the solve).** These faces get no dummies and no landings. For each pair of walls in the face that are not incident (no shared vertex or node, and not a span middle), choose one of 8 directions dir, with sum = 1. Under the chosen direction, both ends of one wall sit at least `RungSpacing` beyond both ends of the other along dir: 4 rows per direction, big-M.
- Prune before the solve: drop directions the face's turning order rules out (the other wall lies on the inner side of the walk), and pairs that are neighbours on one monotone chain.
- Faces Shape draws without dummies (already convex) keep today's linear rows, which are exact there.
- One mechanism handles both planarity and clearance, with no cells in free faces. It is D381's "per-face rows keeping each face's opposite walls apart", written up front with no lazy loop, so it is not generate-and-check (D361).
- It is an exact model of drawability, so a failure names a real upstream promise, not an artefact of a landing choice.
- Cost: an estimated few thousand pairs on 197, a few binaries each after pruning. Whether HiGHS solves that in budget is the open risk.
- It changes D385 and D388 for free faces: Shape stops promising convex cells there and promises only H7 (each face winds once). The runtime Slots model would carry binaries in free faces.

## Recommendation
**B**, measured first behind a flag (`PlanarDrawOptions::pairwise`), with cells and landings left in the code until the measurement decides:
1. Faces 16, 17, 61, 99 alone at 15 s each, with clearance included. Pass: 16 and 17 feasible, and 61 and 99 decided.
2. All 162 faces one at a time, then 197 whole. Report pairs, binaries, rows and time.
3. Kill criterion: 197 whole not solved within 15 s by the end of the second round of pruning. Then fall back to A (D405 as ruled), or to B only in the faces where A's candidate count is above 1.

Why B over A: it is the plainer model (one row family, no cell enumeration). It is exact, which is what an oracle needs (D388 (b)). Its risk (speed) is measurable in one probe, whereas A's risk (the cell enumeration) can't be measured until it is built.

## Prior art checked
- Nöllenburg & Wolff 2011: octilinear metro maps by MILP, with pairwise 8-direction separation of non-incident edges added lazily (we add every pair up front, scoped to one face).
- Tamassia 1987 and rectangular refinement in compaction: the source of the cells approach; for 90 degrees it is exact because ray landings in a rectangular drawing are length-free. That is the property 45 degrees breaks.
- Bridgeman et al. 2000, turn-regularity: the orthogonal condition under which a face's opposite walls are separated by turning alone. It has no 8-direction analogue we know of.

## Reversible
Yes: a flag in the oracle; nothing in Shape changes until a ruling.
