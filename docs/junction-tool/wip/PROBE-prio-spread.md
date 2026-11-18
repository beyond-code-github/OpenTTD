# Probe A4: A3 with the prio join's lanes spread one line wider (2026-11-18, throwaway, ng1 @ 4887288d92 + probe-prio-row.patch + spread, reverted)
Patch: wip/probe-prio-spread.patch (cumulative on probe-prio-row.patch; env PLANAR_PRIOBRIDGE=1 plus PLANAR_SPREAD=1|2). Tables in scratchpad probe-ps/{T2,T3}-s{1,2}.txt, IIS in probe-ps/iis-T3-s{1,2}.txt.

## Change
- JoinEntry(n), only with PLANAR_PRIOBRIDGE set: SPREAD=1 gives 0, 3j-2 (0, 1, 4, 7); SPREAD=2 gives 0, 3j-1 (0, 2, 5, 8). Nothing else.

## Mapped full sets (baselines with the anchor: T2 862.25, T3 1933.25)
- T2 SPREAD=1: 0 of 32 solve; 28 "a clash is not separable" (as A3), 4 "base infeasible" (as baseline).
- T2 SPREAD=2: 0 of 32; all 32 "base infeasible".
- T3 SPREAD=1: 0 of 32; all 32 "base infeasible" (A3 had 27 clash-not-separable).
- T3 SPREAD=2: 0 of 32; all 32 "base infeasible".
- Best: none. The mapped-dump test (plan, signals, checker) was not run: no variant solves.

## IIS (T3 o1t3f1s0, PLANAR_IIS=1 PLANAR_DEBUG=1, run on the test binary directly: pv ignores PLANAR_ONLY)
- SPREAD=1 and 2 alike: base infeasible, IIS = the B1 and B3 (SPREAD=2: B1, B2, B3) chain/segment rows, "run offset B1#10.end.y - B3#7.end.y = 8" (SPREAD=2: 10, and B1-B2 = 6), against "port order" rows CB3#1 -> CB2#3 -> CB1#3, each gap 2.
- Reading: the join exit ports (CB1..CB3) stay two lines apart while the lanes' run offset is now 8 (10); the lane-to-port chain cannot absorb the extra width, so the base model is infeasible before any clash is tested. Spreading the entry lines alone does not fit the port spacing. No rule added.
