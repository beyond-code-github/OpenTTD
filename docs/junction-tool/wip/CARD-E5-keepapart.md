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

## Outcome of B (2026-11-21, NOTES Round 3) and the design of A (orchestrator, D406 fallback)
- B is exact but too slow at scale: whole 197 has 11219 pairs, 44876 binaries and 196k rows, with no answer at 60 s. With a tight M (box 256) single faces take 1.5-6 s. B's use is as a local exact check: it proved faces 3, 6-9, 11, 12, 16 and 17 drawable, where the named landings fail.
- Four bugs in the clearance rows per cell (fixes brief: wip/BRIEF-E5-clear-fixes.md) made every face fail alone. With them fixed, the remaining per-face failures are landing artefacts in about 11 faces.
- Named landings don't compose. A landing that holds face by face still constrains lengths globally: whole 197 with pairwise in those 11 faces and cells elsewhere is still Infeasible (proved in 43 s).

### A: the drawing program chooses each landing (exact)
- **Candidates.** Each dummy d keeps Shape's corner and heading r. Its candidates C_d are the face's track walls and box edges w, in walk order after the corner, such that landing on w leaves both new corners convex (the H12b test, from headings). Landings on dummies are not candidates.
- **Variables.** Order the candidates by walk position from the corner. Cumulative binaries y_d(i) = 1 when the landing is at candidate i or later, with y_d(1) = 1 and y_d(i) >= y_d(i+1). The choice indicator is x_d(i) = y_d(i) - y_d(i+1).
- **Landing rows.** For each candidate, today's landing row is relaxed by M (1 - x_d(i)).
- **Cells.** Dummy d's left cell holds the walk positions p strictly after its corner and up to its landing. A wall or vertex at position p is on that side exactly when y_d(first candidate at or after p) = 1. Write L_d(p) for that, which is linear.
- **Laminar rows.** Dummies don't cross: if d2's corner is on d1's left side, d2's landing must be too. So for each candidate i of d2: x_d2(i) + L_d1(c_d2) - 1 <= L_d1(pos_i), and the mirror for the right side.
- **Clearance rows.** For a vertex at position p and a track wall e at position q in one face, write the today's row, relaxed by M * sum over the face's dummies of |L_d(p) - L_d(q)|. The difference is linear because both are monotone in walk order from d's corner: it equals L_d(p) - L_d(q) when p comes first from c_d. The row binds exactly when no dummy separates them, that is when they share a cell.
- **Why it is exact.** Each cell is a closed walk of fixed headings with turns all <= 0 summing to -8, and every length is >= 0 (landing parts included), so every cell is a simple convex polygon. Clearance inside each cell is then exact, and the cells tile the face.
- **Sizes for 197.** Binaries are the sum of |C_d|, estimated 2-3k against B's 45k. Clearance rows are about the sum of n^2 over faces (vertex, wall), far fewer than B's 16 rows per pair.
- **Risk.** The big-M relaxation is weak. Measure as B was measured (faces alone, then whole), with the B-probe box bound.
- **Kill criterion.** Whole 197 undecided at 60 s.
