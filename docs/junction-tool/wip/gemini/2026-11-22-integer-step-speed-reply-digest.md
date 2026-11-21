# Digest: Gemini on the integer step's speed (2026-11-22, orchestrator)

Prompt: 2026-11-22-integer-step-speed.md. Each point checked against planar_draw.cpp.

## Taken
- **One row per pair (exact).** With a fixed direction each pair wrote 4 rows, every endpoint of one wall against every endpoint of the other (planar_draw.cpp pairwise block), so about 114k separation rows, not 28.5k (Gemini mixed up pairs and rows). A piece's heading is fixed and its length is >= 0, so on the direction's axis s only the "from" wall's highest-projecting endpoint and the "to" wall's lowest can bind: one row is equivalent to the four. Built as `PlanarDrawOptions::sep_extremal` (measurement, default off), probe switch `PLANAR_PROBE_EXTREMAL`.
- **Transitive reduction per face and axis (exact, not built yet).** Rows a->b (min_b - max_a >= g_ab) and b->c imply a->c whenever g_ab + g_bc >= g_ac, because max_b >= min_b (length >= 0). With fixed directions the graph per (face, s) is acyclic when the LP is feasible; removing each edge dominated by a path of other edges keeps the closure (induction on span in topological order). Next if one row per pair is not enough.

## Not taken
- `mip_focus`: not a HiGHS option.
- GF(2) parity bounds on the flex inserts: inserts are axis pieces (always even); our parity result shows no obstruction exists.
- Substituting coordinates out along a spanning tree: HiGHS presolve already eliminates equality-linked variables; not worth hand-coding before measuring.
- Direct rounding schemes (padding, sweep, x-then-y): Gemini agrees none has a proof for octilinear layouts; padding already measured (plateau 44).

## Maybe later
- `mip_heuristic_effort` 0.5 and a rounded LP drawing as MIP start: cheap to try if the row reductions leave the final over 15 s. (JunctionMilp sets mip_rel_gap 0 and threads 1 today.)
