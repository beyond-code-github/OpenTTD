# CARD: what Slots v2 is (E5), now that its behaviour is specified

**Context.** The domain suite (D428, D436) specifies Slots' behaviour: given a contracted drawing, accept exactly what the game allows, and name the refusing rule. Eleven configurations are exact and pinned; V's PSG 197 at its own points is accepted except 5 rebuild artefacts (after D437-D439; the last D438 rows in flight). But nothing has DRAWN a hub since: the suite judges fixed points. Two candidates exist for the solver behind `RowsJudge`:
1. **Track-level, today's program cleaned up.** `CheckPlanarDrawable`'s rows over contracted lines (D432): pieces, ports/box, nodes, crossings, end runs, corridors (D421), grid (D382), the suite's rules (lead, end_node, cross_piece, node turns F6/F8, zero tracks D433/D434, along mode D439), pairwise keep-apart. It is what the suite judges today. Evidence: under contraction, PSG 197's grid final went Optimal in 194 ms (D432's evidence, from a seed). Open: keep-apart sides (one binary per pair today; D431 says "sides by construction") and the modes (trident, zero port line, along) as choices, not references; and it still writes the retired cell rows (`clear`, `land`).
2. **Bundle-level, DESIGN-sss-v2 §3.3.** Spines, gadgets with fixed internal positions, lanes as offset curves of their spine, clearance per free cell. Not built; it leans on cells/dummies that D431 retired, and its rows are not judged by the suite yet.

**Options.**
- A. Slots v2 = (1). Next steps: drop the retired cell rows; make the modes choices (rule first, binary if irreducible); sides by construction where Shape's headings decide them, binaries elsewhere; then solve T2 from Shape's variant (no seed, no pins), judged by the suite's behaviour and the domain; then 197 from Shape's variant (needs the Shape gaps: 7 of V's tracks, TS2's port spacing).
- B. Slots v2 = (2). Rewrite SPEC-slots-lengths for bundles, build test-first against `RowsJudge`. Larger; fewer rows and binaries if it works; no evidence yet.
- C. (1) now as the working Slots, (2) only if (1) cannot meet the seed budget (5 s, D297) on 197.

**Recommendation.** C (= A with B as the named fallback). (1) already passes the behaviour suite and has a 197 timing; (2) is a design with no evidence and partly retired foundations.

**Prior art checked.** D297, D381, D385/D431 (cells retired), D388, D421, D428, D431, D432, D436; DESIGN-sss-v2 §3.3; PLAN-sss-v2 E5.

**Reversible.** Yes: the suite judges either through `RowsJudge`.
