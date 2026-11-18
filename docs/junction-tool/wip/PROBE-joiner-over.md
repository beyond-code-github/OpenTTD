# Probe B: joiner over, main-line lanes S-bend under (2026-11-18, throwaway, ng1 @ 0958786f36, reverted)
Patch: wip/probe-joiner-over.patch (env PLANAR_JOINER_OVER=1 with prio; unset = today). Built exactly as the brief's items 1-5; `pv run build-rel <hub> -j 1`; tables in scratchpad probe-jo/{T2,T3}.txt.

## Mapped full sets (with env)
- T3: 0 of 32 solve: 28 "a clash is not separable", 4 "base infeasible" (same 4 as baseline).
- T2: 16 of 32 solve, 12 "a clash is not separable", 4 "base infeasible". Best o1t3f1s0 967.25 (98x31), 2937 ms; next o1t2f1s0 974.25 (98x34). Baseline best 862.25 (94x29, 733 ms), so +12% area and 4x slower; all o0 variants fail.

## End to end (mapped-dump test: plan, signals, CheckJunctionRules)
- T2 best: PlanPlanarJunction fails "the prio element at 7,10 does not fit"; no layout, the checker did not run, no violations to list. T3: no layout from the composer, so not run.
- Probe A2's failure was at 53,3 / 82,3 (near the merge, first-lane start); this one is at 7,10, the west end of the near-lane coverage region: the prio placement search still finds no site. Not examined further (brief: no new rule).

## No-solve diagnosis (T3 o1t3f1s0, PLANAR_ONLY, PLANAR_IIS, PLANAR_DEBUG)
- Final IIS (two rounds, same core): a lazy clash separation of B1#15 from C2#5 (direction (-1,0), C2#4.end.x - B1#14.end.x <= -2) against the crossing C2#5 over B1#13 (B1#13 >= 1 unit long) and the segment rows of B1#13, B1#14. I.e. the strand named B1 (the main line's far lane) and the C channel's crossing piece: the clash can only be separated by a direction that the crossing and segment rows forbid. No strand/segment rows of the new J or out_j pieces are in the core; no rule added.
- Output: probe-jo/T3-iis.txt. No ASCII picture available from the tests without new code, none included.

## Reading
- Joiner-over as specified does not fix the prio placement (still "does not fit") and gives a larger T2 than today; T3 does not solve at all. A2 (row dropped) remains the smaller layout; the row-room problem is untouched here because the near lane's prio row is still pushed.
