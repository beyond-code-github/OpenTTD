# CARD: where S-bends come from (Shape's offers vs Slots' choice), from V's 197

**Context.** Shape decides headings per stretch (monotone turns); `PlacePlanarShapeFlex` then offers S-bends: one pair each way (h-1 and h+1) per place (D383/D397, flex C D420). Slots sizes each offer (0 = unused; under D432's contracted rows an unused offer is removed). V's PSG 197 needs more than one per place on three tracks (recon: wip/probes/shape/recon-steps.md):
- L3 `0 1 0 -1 0 -1 0` (one-piece stretch: +1, -1, -1), L85 `2 1 2 1 2 3 2` (-1, -1, +1), L72 `7 6 5 6 5 6` (two -1 from the last piece).
To carry any order of n excursions at a place, a place needs n alternating offers each way (k = 3 covers V's 197). D413 (flex on every piece) was rejected for cell costs; cells are retired (D431). With no seed, the open Slots question is the same: which S-bends exist in the contracted rows.

**Options.**
- A. More offers: k alternating offers each way per place (k = 3 from the corpus). Shape unchanged otherwise; Slots decides each offer used or not (a choice per offer: by rule where possible, a binary otherwise). Plain; the cost is more choices in Slots, measured on T2/197.
- B. Slots places S-bends itself: Shape offers none; Slots' contracted rows get, per stretch, up to k step slots it may fill (a choice per slot). Same expressiveness as A, owner Slots; Shape gets simpler, Slots' rows get a slot model.
- C. Shape decides the S-bends (integer excursion counts per stretch in its heading MILP; D395-like, retired). Slots then has no S-bend choice at all (contracted rows fully known). Biggest Shape change; smallest Slots search.

**Recommendation.** A with k = 3 as a measured default: smallest change, carries V's 197, and the Slots measurement (parked brief slots-1) then shows the cost of the extra choices on T2 before Slots is built. If that cost is too high, C (decide them upstream) is the direction, consistent with "choices decided by rule, irreducible ones binaries".

**Prior art checked.** D383, D395 (retired), D397, D402, D413, D420, D431, D432; recon-steps.md.

**Reversible.** Yes (k is a parameter).
