# For Gemini: two 15 s diagnostics on the grid final

## What we ran (PSG 197, the pipeline's integer final with the grid rows, every variable integer, 15 s each)
1. **Soft sep only** (your suggestion): sep rows elastic, slack cost 1, piece length cost 0.01. No integer point in 15 s. LP bound 4.76, root cuts to 19.44. Our mistake: at 0.01 per unit of length a slack of 1 pays for any 100 units of length, so the LP traded slack for length; that bound says nothing about conflict.
2. **Soft everything except the game rules**: every row family elastic (sep, curve, corridor, bay, node, crossing, deck, port, flex...) except piece (point = point + step x length), box, and the grid rows (2g + b, b one binary per axis); slack cost 1, length cost 0. LP bound 0. **Still no integer point in 15 s**: 33289 x 34155 after presolve (2704 integer, 29927 implied integer, 2 binary), 3563 cuts, root node only, primal bound inf.

## Our reading
Run 2's hard rows are satisfiable track by track (each track can be laid on the grid alone), so an integer point exists. So the grid is not infeasible: HiGHS's primal heuristics cannot reach the lattice. Without grid rows the LP point is near-integral and randomized rounding at the root succeeds (1858 at 17 s); with them each g sits at a half and rounding fails.

## Corrections to your last answer
- "+7-10% is the real physical cost of the grid": 1999 was a dual bound under the sides the off-grid LP fixed, not the grid's cost.
- Diagonal parity is not always local: under our flex points (pairs h, h+-1, h on most stretches) the axis pieces next to a diagonal may have length 0, so "odd on a turn, even on an S-bend" holds only where both neighbouring axis runs are known positive. Our Slots layer finds the forced parities by GF(2) elimination over the equalities instead.

## Questions
1. Given run 2 (feasible, LP 0, yet no integer point in 15 s), is the gap HiGHS's heuristics on general integers with parity structure? What would find a grid point here in a second or two:
   - our own rounding: snap each axis run's centre line to its parity, then repair the diagonals and the steps by a small LP or a shortest-path / difference-constraint pass, and hand the result to HiGHS as a full MIP start;
   - the tile-unit formulation (run centre lines as whole-tile variables, diagonals 2h + p with forced p, right-hand sides rounded), so the LP's vertices are nearer the lattice;
   - something else (HiGHS options for primal heuristics on general integers: feasibility pump, RINS, sub-MIP effort)?
2. For a full MIP start: HiGHS rejected our earlier partial start (lengths only) with "Presolve: Infeasible" in its start sub-MIP. Does a complete start (every variable, including g and b) avoid that, and does HiGHS 1.16 then use it as an incumbent directly?
3. Which of these would you try first for a grid-legal 197 final inside about 5 s?
