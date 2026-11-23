# Review request (follow-up): the first stage of our junction-drawing integer program

Context: this follows your review of 2026-11-23 ("an integer program for drawing railway junctions on a tile grid"). Recap: OpenTTD junctions on a square tile grid, with track pieces in 8 headings (axis or 45-degree). Upstream layers have fixed the topology and every track's sequence of piece headings. "Slots" chooses the piece lengths, and so the points, so that the drawing is legal and compact. Keep-apart means every pair of non-adjacent walls of a face is at least one lane apart. It is a disjunction over 4 separating candidates (2 axes x 2 sides), with one binary each and big-M rows. S-bends: the heading layer offers optional sideways steps ("flex"). Each is written as flank, insert, flank: the insert has heading h±1, the flanks have the stretch heading h, and the solve decides which are used.

Two-stage plan (yours, adopted): stage A picks which S-bends are used. Contraction removes the unused ones (zero-length pieces dropped, equal headings merged). Stage B is the integer, contracted, on-grid solve with keep-apart, checked by a ground-truth tile checker.

## What we measured today (hub T2 unless stated; HiGHS; 15 s cap per solve)
1. Your step-0 checks worked. Round 1 of stage A's lazy keep-apart loop (51 close pairs, 204 binaries) found nothing in 15 s. With a tight coordinate bound (64 half tiles instead of 4096, so M 256 instead of 16k), the solve was Optimal in 4.3 s. With a zero objective it was 4.5 s, and with both 0.58 s. A MIP start made no difference. Grid parity was not the cause (0.86 s without it).
2. Fixing every new pair's side from the drawing that broke it is Infeasible in 30 ms, with or without the grid.
3. With free binaries, a tight bound each round and a zero objective, the lazy loop does NOT converge. Round 0 has 79 failing pairs; then about 18 new failing pairs per round from round 2 on; the rows reach 205 pairs (820 binaries) and round 7 hits the cap. Variants:
   - With the length objective, round 1 drops the failing count from 51 to 39, but round 2 (90 pairs, 360 binaries) times out.
   - Adding "near" pairs (margin under 2 tiles) adds 1010 of 1945 pairs at once, and round 1 times out.
   - The small hub T1 converges in 3 rounds (23 of 811 pairs, 0.33 s). A 136 ms polish with the length objective, MIP-started, shortens it from 134 to 79 and stays legal.
4. The contracted problem is about 3x smaller (T1: 318 pairs against 811; with all rows it solves in 251 ms). The engineer has therefore ruled that keep-apart lives only in stage B. Stage A has no keep-apart rows and picks S-bends by the length objective alone. When stage B is infeasible, dropped S-bend offers near the conflict come back.
5. New problem: stage A's drawing is not contractable. Stage A lets inserts AND flanks be 0. A run of zero pieces can then make two non-zero pieces more than one eighth apart adjacent after contraction (turn rule F3: consecutive pieces are at most 45 degrees apart). The same happens at a node: a leg's first non-zero piece is more than 45 degrees from the trunk's (F6), or two legs lead on one heading (F8).
   - T1: 3 tracks and 4 nodes break. T2: 4 tracks and 7 nodes break.
   - Typical cases: an insert at h1 whose following flank (h0) is 0, right before a Shape bend to h7. Or a run of 7 zero pieces (alternating flanks and inserts) between a used insert and a piece two eighths away.
   - Every case is this one mechanism. With no such rule, stage B (built from the contraction) is Infeasible before any keep-apart row.
6. The fix we were about to build:
   - A 0/1 "used" variable u_k for every piece that may be zero, linked by len_k <= U·u_k and len_k >= u_k (integer lengths).
   - F3: for any i < j on a track, with headings more than one eighth apart and only zero-able pieces between them, u_i + u_j - Σ_{i<k<j} u_k <= 1. The same row over trunk-then-leg at a node gives F6. For F8, two legs cannot both lead with one heading through zero pieces.
   - This is exact for the contraction rule, but it adds about one binary per zero-able piece (T2: about 300).

## Questions
1. Is a two-stage split still right, given that stage A must now carry the turn rules (with ~300 binaries) but not keep-apart? Or should stage A choose S-bends some other way: by rule from the headings, greedily, or as an LP with rounding?
2. The "used" binaries and run rows: is there a tighter or standard formulation for "no two adjacent non-zero pieces in a sequence turn more than X, zeros transparent"? For example, a flow or state-machine encoding (a state per piece holding the current heading), or simply forbidding zero flanks next to a used insert.
3. Would you instead forbid zero-length flanks altogether (each flex group is all-or-nothing: an unused group has insert 0 and flanks merged; a used group has both flanks >= 1)? That removes most zero runs at the cost of some compactness. Is that cheaper and sound?
4. Keep-apart in stage B only: with T2's lazy loop not converging in stage A (about 18 new pairs per round), what would you expect in stage B? Is there a standard way to make lazy separation rows converge, such as adding the k nearest pairs per face, adding a pair's whole neighbourhood, or a movement-limiting (trust-region) objective between rounds?
5. Given the 5 s target for a whole solve and a real hub about 8x T2, which of these directions scales, and what would you drop?
