# Update for Gemini: stage A on T2 with the flank rows

Your two checks ran. One correction to your last diagnosis first: the flank rows have no binaries. Each row is `64·flank − insert ≥ 0` over two integer lengths. Where an insert is held used, we now write it as a plain bound, flank ≥ 1.

## Results (T2 stage A, no keep-apart rows, length objective, 15 s cap)
| run | result |
|---|---|
| no flank rows (grid on / grid off) | Optimal 2.4 s / 2.6 s |
| 159 flank rows, K = 64 (grid on / off) | no solution in 15 s / 15 s |
| 159 flank rows, K = 8; or zero objective | no solution in 15 s / 21.8 s |
| LP, slack on the flank rows only | Optimal 10 ms, total slack 0 |
| LP, all rows hard, grid off | Optimal 25 ms |
| integer, slack on the flank rows only | no solution in 15 s (the slack mode is slow on integer problems in general here) |
| flank rows dropped on the long all-flex tracks D1, D2 and F1 | no solution in 20.6 s |
| **your check 2:** inserts locked from the 2.4 s drawing (used ≥ 1, unused = 0), flank rows as bounds | **Infeasible in 76 ms** |

The locked solve's conflict (101 rows) holds:
- flank rows on 5 inserts: D1 1 and D2 1 (both sides; piece 1, so the node-side flank is piece 0 at a split), A2 7, B1 18 and F2 11;
- the piece rows of most tracks;
- the E and S port gaps;
- nodes 0, 2, 3, 6 and 10;
- crossings 6, 11, 13, 14 and 18;
- decks 2 and 3 with their end runs.

The D1/D2 node-side rows are real requirements: a used insert with a zero node-side flank makes the leg leave the split two eighths from the trunk (F6).

## Our reading
- The cause is the combinatorial choice of S-bends, not big-M and not grid parity.
- Without turn rules, T2 steps sideways right at the D1/D2 split to line up its crossings and decks. Under the turn rules each such step needs a flank, which pushes the step one unit along, and the crossings and decks nearby have no room for it.
- The lock test rules out only the one S-bend combination that was chosen without the turn rules. We cannot yet tell "no legal combination exists" (an upstream gap: Shape's S-bend offers near that split) from "a legal combination exists but HiGHS does not find it in 15 s".

## Next on our side (running now)
Local freeing on the locked model:
- (a) used inserts stay used and every unused insert becomes free;
- (b) additionally free the 5 inserts the conflict names.

If (b) is feasible quickly, a legal combination exists nearby and the problem is search. If it is infeasible quickly, that region is the upstream gap.

## Questions
1. If the free model is feasible but hard to search: what is the standard way to choose sequence-dependent options like these, where a used S-bend needs its flank? For example: a used/unused binary per insert with the flank linked to it (u ≤ flank, insert ≤ M·u), branching priority on those binaries, local branching or neighbourhood search from the turn-rule-free drawing, or a lexicographic/symmetry-breaking order on the alternating offers.
2. If it is infeasible: the turn rules force a flank of at least one unit next to every used insert near a node or bend. Would you move that into Shape's offer placement (no offer whose insert would have to sit flush against a node or bend), or keep the offers and let Slots fail?
3. Anything in the numbers above that points elsewhere?
