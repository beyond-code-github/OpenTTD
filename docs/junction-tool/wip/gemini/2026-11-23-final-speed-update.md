# For Gemini: update, and where the integer final's time goes

## Done since your last answer
- D422 is adopted and built in as the drawing pipeline:
  - discovery LP rounds: IRL1 insert prices (0.05 in round 1, then 0.1 / (s + 0.5)), a box price of 0.01, keep-apart directions re-picked to slack 0;
  - commit the inserts at >= 1 half tile, with curve rows on their bends, the rest held at 0;
  - commit rounds, at most one prune;
  - an integer final on the sides the last round proved.
- On PSG 197: 12 committed, 2 pruned, curve-legal at slack 0; integer final Optimal in 17 s (box 121x188, length 2956; the expert 101x124, 2207, about 50 steps).
- A one-lane T is not drawable with keep-apart: separation slack plateaus at 3 around a prio row's closing step. We are diagnosing it separately.

## The 17 s, from the HiGHS 1.16 log
- MIP 36867 rows x 5153 integer columns (0 binary), presolved to 18366 x 2189 in 0.7 s.
- Root LP bound 1804.25 at 1.8 s.
- **Root cut loop from 1.8 s to 15 s** (1774 cuts in the pool, 133 in the LP), bound 1857.5.
- Randomized rounding then finds 1858 at 17 s. One node, 3875 LP iterations: no branching at all.

## Tried, no effect
- mip_rel_gap 0.01 and 0.03; mip_detect_symmetry false; feasibility jump on (with and without mip_max_improving_sols 1): all 16.9-17.0 s.
- MIP start from the last LP round's whole lengths (1360 values, a partial start): the start sub-MIP is "Presolve: Infeasible".
- Feasibility only (no objective): 14.9 s; the cut loop still runs 1.4 to 13.5 s (2744 cuts) before rounding finds a point.

So the cuts are what bring the root LP close enough to the integer lattice for rounding to succeed. The time is the relaxation's distance from the lattice, not the objective or the proof.

## The lattice
- Points are integer half tiles.
- An axis piece's length is 2k with k integer.
- A diagonal piece moves (+-1, +-1) per unit.
- Game grid (not yet written in this program): every point has x + y odd; N/S pieces sit on odd x and E/W pieces on odd y.
- Every bend is 45 degrees, so each diagonal's length parity follows from its neighbouring headings: odd on a turn, even on an S-bend.
- We already have, in another layer, a GF(2) elimination that finds the forced parities and writes each forced length as 2h + parity.

## Questions
1. Would writing the game lattice explicitly (forced parities substituted: every length 2h + p with p known, every point's grid bit fixed) be expected to cut the root cut loop substantially? Or would HiGHS still need about the same cuts, given that the LP relaxation over h is still fractional?
2. Since HiGHS 1.16 has no option for the number of root cut rounds, what else shortens a root cut loop that is doing the work of making the LP near-integral? Candidates:
   - fewer rows (transitive reduction of the separation rows);
   - a different LP solver at the root (IPM, then crossover);
   - a structured rounding heuristic of our own: round lengths in tile units along each track, then repair with the fixed directions.
3. Which would you try first, to reach about 5 s for the final on this model?
