# Probe A2: prio over a doubled bridge, near lanes' prio row dropped (2026-11-18, throwaway, ng1 @ 0958786f36 + probe A patch, reverted)
Patch: wip/probe-prio-bridge2.patch (cumulative: probe A plus the change; env PLANAR_PRIOBRIDGE=1, unset = today). Runs via pv run build-rel <hub> -j 1; tables in the scratchpad probe-pb2/{T2,T3}-probe.txt.

## Change
- 2-lane Join: prio row for S_near not pushed; n-lane Join: rows for S[k], k >= 1 not pushed. Far lane's row and Block227 rows kept. Nothing else.

## Mapped full sets
- T2: 28 of 32 variants solve (4 "base infeasible", same as baseline). Best o1t3f1s0 684.25 (67x29), 849 ms. Baseline best 862.25 (94x29). Next o1t2f1s0 688.25 (67x32).
- T3: 28 of 32 solve (same 4 infeasible). Best o1t3f1s0 1534.25 (100x39), 1605 ms. Baseline best 1933.25 (138x39). Next o1t1f1s0 1542.25 (100x42).
- So the clash of probe A is gone; the dropped row also shrinks the area a lot (T2 -21%, T3 -21%): the row's 2P+2 reservation was costing area.

## End to end (JUNCTION_COMPONENTS_TOPOLOGY=T2|T3, the "dump a mapped component layout" test, which runs the plan, signals and CheckJunctionRules; the plan tests themselves not run)
- T2 and T3: no layout. PlanPlanarJunction fails: "the prio element at 53,3 does not fit" (T2), "the prio element at 82,3 does not fit" (T3), at the best variant. So the checker did not run (no violations to list, G9c still untested on a plan) and no prio placement exists: the signal pass's search (D345) finds no place for the prio element on the main line, as the main line there is a doubled bridge with joiner diagonals crossing it.
- Outputs: probe-pb2/T2-e2e.txt, T3-e2e.txt.

## Reading
- The composer-level coverage problem is solved by dropping the row; the open problem moved to the signal pass: the prio element placement search has no fitting site (53,3 and 82,3 are the first-lane start, near the merge side). Which variant/site constraints reject it was not examined (brief: no further rule).
