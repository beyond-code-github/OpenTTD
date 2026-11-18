# Probe: hard anchor in the mapped composer (PLANAR_ANCHOR=1), ng1 95fe3d99c7

Patch: probe-anchor.patch (X0 = 2a, Y0 = 2b with a, b integer columns; pin is a window 4096 <= u <= 4097 on both axes for the first port's first line's point; box rows, in/out port sides, width/height as X - X0, Y - Y0, route geometry shifted by X0, Y0 in the mapped caller only).
- Pin (P0, P0) as a point is infeasible on every variant: a strand starting on a diagonal needs start x+y odd (`AddPiece` parity), and today's classes come from the edges (x even at W, y odd at N = 1). Hence the window and the even X0, Y0.
- With the window every hub solves.

## Areas (pv, per variant), env unset vs set
- T2: 32 variants, 28 solve both ways; every area and reason identical. Best 862.25 (o1t3f1s0) both ways.
- T3: same; every area and reason identical. Best 1933.25 (o1t3f1s0) both ways.
- LL2: the 12 tags pv picks differ between the runs (the PLANAR_DRY build list is not the same with the env set; not investigated). Every tag in both lists has the same area (o0t0f0s0..s3, o0t1f0s0/s2/s3, o0t2f0s2, o0t3f0s0). Best of the full test 1347.25 (o17t0f1s0) both ways.

## Times (full mapped test wall; best-variant solve ms)
- T2: 2.83 s -> 1.75 s (best variant 2815 -> 1736 ms). T3: 4.08 s -> 3.25 s (4062 -> 3233 ms). LL2: 92.15 s -> 74.06 s (92144 -> 74057 ms).
- pv totals: T2 4.1 -> 3.3 s, T3 5.5 -> 4.3 s, LL2 10.4 -> 8.6 s. LL2 per-tag ms moved both ways (s1/s3 variants 7.2-8.4 s -> 5.5-5.7 s, s0/s2 about equal). Single runs, no repeats; the area-limit CHECK in the three tests fails in both modes (as today).

## Defect found: the box low edges are not tight
Y0 is only a lower bound (`p[1] - Y0 >= 1`); nothing in the objective pulls it up, so it stays at 0 and the reported height is the distance to the pin (T2 94x2073, T3 138x2081, LL2 87x2100, against 94x29, 138x39, 87x59 today). Width came out right (a W port ties X0). Areas are untouched (the outline is translation invariant), but the route shift by Y0 is therefore wrong: T2 routes do NOT equal today's (y tiles about 2040 too high); I did not dump and diff them, the height already shows it. A usable anchor needs the low edges tied to the layout (an N port ties Y0; none of T2, T3, LL2 has one on its mapped N side, or a row that Y0 equals the lowest point, or a cost on Y - Y0).
