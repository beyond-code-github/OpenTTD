# Gemini on cells with optional bends: digest (orchestrator, 2026-11-22)

- **Q1, binary-free exact clearance:** no. Segment clearance is a 3-way disjunction (before / on / after the segment). Closest literature: constraint (visibility) graph compaction (Klau & Mutzel; Eiglsperger & Kaufmann): walls push only where their intervals overlap along a sweep axis, order fixed by topology; octilinear form uses the 4 axes x, y, x+y, x-y. A principled long-term form, close to D381 (keep-apart rows read from the map).
- **Q2, cost:** proposes (c) pairwise only on pairs touching a flex group (vertex or wall), static cells for the rest with their line rows to and from flex groups dropped; guesses a few hundred binaries instead of 38.7k (to measure). Its zero-binary "longitudinal along h" exemption is our pocket rule (SPEC-shape §8.9), which measured NEGATIVE (FS-d): discounted.
- **Q3, mechanism:** confirmed (a used step makes the cell non-convex; the line beyond it is a ghost wall).
- Action: measure (c)'s binary count on 197 (D402 and C), then a K-E5 card to the engineer.
