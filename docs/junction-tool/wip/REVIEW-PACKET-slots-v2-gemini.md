# Review request: an integer program for drawing railway junctions on a tile grid

## The problem
We draw railway junctions (OpenTTD) on a square tile grid. Track pieces run in 8 headings (axis or 45-degree diagonal). Upstream layers have already fixed the topology (which lines split, merge and cross, and which crossings are bridges/tunnels), and the HEADINGS of every track piece (a polyline per track: a sequence of headings). What remains ("Slots") is to choose every piece's LENGTH, hence every point, so that the drawing is legal in the game and compact.

## The model today
- Variables: per track piece a length (integer; axis pieces in whole tiles, diagonals in half tiles); points follow by summing pieces from fixed port positions on a box edge; the box size.
- Exact rows (all linear): piece closure, ports on the box edge with exact lane spacing, node points shared by legs, crossings on the right pieces (a crossing parameter per crossing), tunnel/bridge spans straight with end runs, prio rows as offset curves of their lane (fixed one-lane offset), grid parity (points on tile-edge midpoints: an integer parity substitution v = 2h + p), and a dozen local game rules (each certified by an exhaustive offline test suite against a ground-truth tile checker).
- Keep-apart (the hard part): every pair of non-adjacent walls (pieces) bounding a face of the planar layout must be at least one lane apart. Two segments apart = a disjunction over separating directions: today 4 candidates per pair (each wall's normal, two sides), one binary each, big-M rows, one "pick one" row. Pairs are all non-adjacent pairs per face: O(walls^2) per face.
- S-bends: the heading layer offers optional S-bends ("flex pairs"): a sideways step that may be used or not; a used one has length >= 1, an unused one is removed (we cannot allow length-0 pieces: rows over zero-length pieces are not sound).

## What we measured
- Pair counts: hub T1 811 wall pairs, T2 1945, T3 3399, a real champion hub (197) 15,781 (x6 when 3 S-bend offers per place are allowed). With every pair's binaries, no hub solves in 15 s (CBC/HiGHS-class MILP), not even T1.
- Sides cannot be fixed upfront for most pairs: rules from the planar layout (face walks) decide ~30% of pairs safely on the real hub; convex-cell refinement fixes the half-plane but not the axis that carries the spacing (staggered parallel walls are legally close side by side because they are apart lengthwise).
- Lazy rows work on the small hub: solve with no keep-apart rows, add rows only for pairs that come too close, repeat: T1 converges in 1-3 rounds, only 1-15 of ~300 pairs ever get rows, the final integer solve takes 6-17 ms, and the result is legal.
- On T2 the integer solve is slow: with no keep-apart rows it takes 2.4 s; adding the 51 pairs that came too close (204 binaries) gives no incumbent in 15 s. The LP relaxation with ALL keep-apart rows is feasible (0.6 s). The big-M uses a coordinate bound of 4096 half tiles (M ~ 16k); earlier, with a tight box (M ~ 500), single faces that were undecidable solved in under a second. The objective minimises total length; we only need a legal drawing (compactness is secondary).

## The plan for tomorrow
1. Stage A: an LP (continuous lengths, no grid), lazy keep-apart rows, each added pair's side read from the drawing that broke it (freed to binaries if a round goes infeasible). Output: which S-bends are used, and a side for every pair that ever got rows.
2. Contraction: remove unused S-bends; keep any flank whose removal would break a turn rule at a node.
3. Stage B: integer, contracted, grid parity on; stage A's sides fixed; lazy for any pair still too close; then the ground-truth checker on the result.
4. Optional: one-axis-at-a-time compaction (freeze y, compact x as an LP where every separation is one inequality from the current order, then swap) to tighten stage B's drawing without binaries.
5. Before all that, cheap checks: a tight coordinate bound (from stage A's drawing) for every big-M; a zero objective (feasibility only); stage A's drawing as a MIP start.

## Questions
1. What most likely makes the integer stage slow with only ~200 binaries: the big-M size, the integer lengths with parity, the objective, symmetry, or something else? What would you measure first?
2. Is there a better formulation of "two octilinear segments at least d apart" than 4 big-M candidates per pair (e.g. indicator constraints, a disjunctive hull, a separating-axis formulation, or exploiting that one segment is axis-aligned)?
3. Lazy rows: is this the standard row-generation pattern you would use here, and how would you avoid the side guesses (from the previous drawing) going wrong round after round?
4. Is "LP to pick sides and S-bends, then a contracted integer solve with those sides fixed" sound as a pipeline, or does fixing sides from an LP risk unrecoverable infeasibility? What would you do when stage B is infeasible with stage A's sides?
5. One-axis compaction: worth it here, given 45-degree pieces couple x and y (a diagonal's length is fixed by whichever axis is frozen)?
6. Anything in the plan you would drop, reorder, or add?
