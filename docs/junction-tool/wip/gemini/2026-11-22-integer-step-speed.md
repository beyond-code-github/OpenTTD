# For Gemini: making the integer step fast once directions are fixed

## Setup (recap)
- We draw an OpenTTD rail hub as an LP/MIP on a half-tile grid, solved with HiGHS 1.16 (MIT; CP-SAT is ruled out by licence and was slower anyway). The shape is fixed: per track a heading list (8 directions) and the piece each crossing sits on. Lengths are the unknowns: an axis piece is 2k half tiles (k integer), a diagonal piece moves (+-1, +-1) per unit (integer). Points are integer half tiles.
- **Flex points:** on free stretches we insert optional bend pairs (h -> h+-1 -> h), each an axis insert of length >= 0, so a sideways step may appear or not. On PSG 197 (an expert hub, 101 tracks) that is 654 inserts.
- **Keep-apart (pairwise):** in each face, every non-adjacent wall pair keeps a spacing along one of four (axis, side) choices. The choice per pair is read from an earlier drawing (bounded rounds of an LP that puts slack only on separation rows, until slack 0). So the final program has **no binaries**: a pure integer program, 28.5k pair rows, plus exact rows (port gaps, node shapes, crossing and deck rules).

## What we measured (PSG 197, unpinned, the same fixed directions)
| final | result |
|---|---|
| all continuous (the LP) | Optimal, 6.4 s |
| integer points, axis lengths continuous (any integer half tile) | Optimal, 40 s; nothing at 15 s |
| fully integer (axis pieces whole tiles) | no incumbent in 60 s |
- Without optional flex (the expert's steps given as fixed bends, 1.5k pairs) the fully integer final solves in 6.4 s.
- The LP drawing is tidy (a small length tie-break): thousands of pairs sit at margin exactly 0, and 66 of the flex inserts are fractional (1.5, 2.5 tiles).
- We showed parity is no obstruction: on the game grid every length's parity follows from the headings and telescopes to 0 around any face. So the integer program is feasible; it is the search that is slow.
- Target: the whole solve under 5-15 s.

## Questions
1. **Rounding with a guarantee.** Given the LP drawing with directions fixed, is there a known way to build an integer drawing directly? For example:
   - pad each separation row in the LP by the worst rounding error so that rounding lengths cannot break it (our padded rounds plateaued at slack 44, because exact port and node geometry cannot absorb padding);
   - round lengths in a fixed order along each track and push neighbours (as in a compaction sweep);
   - solve per axis (x then y), as orthogonal compaction does, even though diagonals couple the axes.
   Which of these has a correctness proof for octilinear layouts?
2. **Fewer rows.** Most of the 28.5k pair rows are far from binding. Within a face, is a pair's row implied by others (transitivity along a chain of walls, a visibility or sweep-line reduction as in constraint-graph compaction)? Which pairs can be dropped with a proof, not by checking the result afterwards (we do not allow clash-and-resolve loops)?
3. **Solver use.** For a binary-free integer program of this shape (difference-like rows plus diagonal rows x + y >= c, many equalities), which HiGHS settings, or which reformulation (e.g. variables in tile units with the parity substituted out, or rotated coordinates where diagonals become axis moves), most help it find a first incumbent? Is a rounded LP drawing as a MIP start likely to help when it violates a few hundred rows?
4. **Which would you try first,** given a 15 s budget and no loops?
