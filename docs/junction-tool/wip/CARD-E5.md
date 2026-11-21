# CARD E5: the feasibility oracle (orchestrator, 2026-11-21)

Implements PLAN-sss-v2 E5 (Pete: "lets get to e5 asap"), DESIGN-sss-v2 §3.3 (Slots' rows) and §4 G-feasible (D388 (b): the full feasibility program as a TEST oracle of Structure's and Shape's promises). Inputs: Shape's output as it stands after E3-S2 (headings, flex pairs D397, spines, cells and dummies H12), Structure's rule fields (E2), SPEC-slots-lengths §8.1-§11 (v1 evidence for the mechanisms: flex, offset curves, chords).

## The question it answers
Is Shape's shape drawable on the tile grid with every Slots rule kept? On PSG 197 that is the one big open question for solving a given hub. A failure names the rows that conflict (IIS), which names the missing upstream promise; the fix goes upstream (D361, D367).

## The program (integer, no objective; rows by family, all written from the map and Shape's output before the solve, D381)
1. **Lengths:** per piece a length in half tiles (axis >= 2 and even, diagonal >= 1); per flex pair a size >= 0 (0 = no step, D397); per dummy a length >= its gap.
2. **Closure per cell:** each convex cell's walls (pieces, parts of pieces cut by a dummy's landing, dummies, box edges) sum to zero in x and y.
3. **Landing:** each dummy ends on its named wall: the landing point at u along it, 0 <= u <= the wall's length (linear: the wall's heading is fixed); landing on another dummy stays allowed (today's H12b; H16 not imposed: it buys nothing for linearity).
4. **Clearance per cell:** every vertex of a convex cell lies at least the tile rule's gap inside the supporting line of every wall it is not on (linear because the cell is convex; walls meeting at a node or a crossing are incident, so exempt). The node proof (`[planarnode]`) says legs need nothing more than this past the node tile.
5. **Exact:** port pins and side gaps (`side_gap`); beside tracks (copies, rows) as offset curves of their parent with miter rows (SPEC-slots §11); per span the crossers' positions with deck gaps per run and the span's middles within `limit` (H6, D396); end runs; coverage exact (D384, `rules.coverage`).
6. **Lower bounds** from `rules` (E2): cross, node_leg with D110's binary per consecutive split pair (0 = trident, D400), bay; curve spacing as an oracle count first (SPEC-slots §8.3), rows later.
7. **Grid parity** (D382): forced parities before the solve, ported from the probe.

## Tests (outside-in)
- **Outer:** "Shape's PSG 197 shape is drawable": the program on Shape's own 197 variant is feasible (or its IIS is printed). Same on T2, T3, LL2 top variants.
- **Known solution (the rows' own check):** V's real 197 with V's real lengths satisfies every row. A row V breaks is a wrong row (or a rule V breaks on purpose: named). Needs V's piece lengths from the save: the extractor's X7 extended to lengths (corpus side, parallel).
- **Build order:** lengths + closure + exact + parity on T1, then T2 (feasibility), then landing and clearance, then 197; each family's rows test-first on a hand case where only that family is violated.

## Decision for Pete first (it decides whether V's 197 can be a known solution at all)
- **Context:** D397 put flex points only on free stretches between two crossings, because steps at ports and next to nodes were to stay "inside components". D401 then dropped components. So V's other 41 steps (port fans and close-ups, S-bends a few tiles from a split or merge) now have no owner: no layer can produce them, and V's real 197 cannot satisfy the rows.
- **Options:** A. Extend D397's rule: a flex point (both sides, sized >= 0) on every one-piece stretch outside spans and crossing lattices, port and node legs included. B. Keep D397 as it is and let the node rule own steps on legs (a leg may S-bend once before its next event), a separate mechanism.
- **Recommendation:** A (one mechanism, already proven on cells and the route cone; the node proof shows legs need no special clearance).
- **Reversible:** yes.

## Also pending (asked the same day)
- Channel 61 (D23c, D12c): the legs of merge D123 converging under the A tunnels; proposed: legs of one node are exempt from the spine (H14), then H14 joins the promise.

## Measured items added (2026-11-21)
- **Shape's objective vs compactness:** Shape minimises turning only because it has no lengths; V spends turning for compactness (V's 197 turning 80 at Shape's level vs Shape's own 49). Once 197 draws: compare Shape's drawn footprint with V's; then decide (orchestrator recommendation: Shape hands down several differing shapes and Slots picks by footprint).

## Progress
- E5-1 bdd26426f2: points, pieces, flex, box, ports, nodes, crossings, spans: T1, T2, Shape's 197 drawable.
- E5-2 1ba107d8b5 (Fix: dummy landing on a dummy split at the wrong wall), 157d544ff1 (dummies, landings, clearance per cell, span middles exempt; `PlanarDrawOptions`).
- D404 6485ea8c56 + 312c19aa48 (no cusps at topology nodes; V's switch-tile curves recorded on the legs).
- Open: T1/T2 with clearance wait for the corridor family (a prio row's split is an offset-curve cusp); 197 infeasible already with landings alone (no clearance): the H12b landing rule (first facing wall, from headings only) may name walls no lengths can reach: per-face diagnosis running (wip/probes/e5land).
