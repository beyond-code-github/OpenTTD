# Gemini on turn-regularity: digest (orchestrator, 2026-11-22)

- **References (to verify before citing):** Patrignani, "On the Complexity of Orthogonal Compaction", Comput. Geom. 19(1):47-67, 2001 (NP-complete in general); Bridgeman, Di Battista, Didimo, Liotta, Tamassia, Vismara, "Turn-Regularity and Optimal Area Drawings of Orthogonal Representations", Comput. Geom. 16(1):53-93, 2000 (turn-regular faces: visibility fixed, two constraint graphs, polynomial, no disjunctions).
- **Octilinear:** no known analogue of turn-regularity; 45 degree pieces couple x and y, so even convex faces can switch which wall blocks a ray; what works is a decomposition into cells with fixed landing walls (our convex cells with H12b landings).
- **(B) optional steps in constraint-graph compaction:** hard; a step that is 0 or positive creates or removes kitty corners: Patrignani's core.
- **(A) steps decided upstream:** exact and binary-free with our convex-cell refinement, once the used steps are refined as real bends.
- **Choosing steps:** min-cost flow on the dual cut graph (totally unimodular; Tamassia 1987; Nöllenburg & Wolff 2011 for octilinear maps); expert patterns as defaults (corpus authority).
- Action: card resolves to (A); probe on 197 with V's 50 steps as the used steps, cells refined with them, others 0; pending the engineer's go.
