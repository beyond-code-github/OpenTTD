# Probe A3: the prio row as a gapped offset copy of the near lane (DESIGN-tracks 13.1), main line over (2026-11-18, throwaway, ng1 @ e4b50580e1 + probe-prio-bridge2.patch, reverted)
Patch: wip/probe-prio-row.patch (cumulative: A + A2 + this; env PLANAR_PRIOBRIDGE=1, unset = today). Runs: pv run build-rel <hub> -j 1; tables in scratchpad probe-pr/{T2,T3}-probe.txt, IIS in probe-pr/iis-T3.txt.

## Change
- n-lane Join, prio branch, each near lane S[j] (j>=1), under the env: after its last segment, T = 2*(-sigma)*Right(h); the segments added in this call minus the d0 of each Double (the gaps) form maximal runs, each a strand `prio:<S[j]>#<r>`: start tied to the run's first segment's a + T, one AddSeg(s.d, 0) per segment with L equal to the lane segment's L. The far lane's fixed row and Block227 unchanged. The box loop (M.order, ~1752) covers these strands (created before it); the clash check treats them as unrelated strands.
- Literal brief: AddSeg(name, s.d, 0) leaves the default cost 1.0 per half-tile (today's rows use 0); not changed, so areas would carry the row lengths in the objective (moot: nothing solved).

## Mapped full sets
- T2 (32 variants): 0 solve. 28 "a clash is not separable", 4 "base infeasible" (o0/o1 t3 f0, as baseline).
- T3 (32 variants): 0 solve. 27 "a clash is not separable", 1 "infeasible with topology sides" (o0t2f0s1), 4 "base infeasible".
- A2 had 28 of 32 solving; A3 solves none. Best: none.
- The mapped-dump (plan, signals, checker) test was not run: no variant solves.

## IIS (T3 o1t3f1s0, PLANAR_IIS=1 PLANAR_DEBUG=1)
- Rows: row run 1 of lane B1 (`prio:B1#1`, the run after the doubled stretch: its tie at L1035 puts its start 2 half-units off B1#13's end, then segs #0 (axis), #1 (the turn piece), #2 (axis, L equal to B1#16.L)) against lane B2's second copy `B2''` (B2''#1) and B2#14/#15: "sep B2''#1 prio:B1#1#3 dir(-1,-1) >= 2" and "sep B2#15 prio:B1#1#2 dir(1,0) >= 2", on top of the parity/chain rows of B1/B2 (run offset B1#10.end.y - B2#10.end.y = 4, ties of B2''#1 to B2#14).
- Reading: the offset copy of B1 sits on the side where lane B2 and its copy B2'' run (B1 and B2 are 4 half-units apart, the row takes 2 of them, the copy of B2 steps onto that line); the row run after the gap cannot be separated from B2''. No rule added.

## Orchestrator reading (ASCII T3 o1t3f1s0 today vs A2: wip/ascii-T3-o1t3f1s0-base.txt, -A2.txt, the B join, top rows)
- In a prio join the main-line lanes sit two lines apart; the line between two lanes holds BOTH the outer lane's prio row and the inner lane's doubled-bridge copy (the copy steps toward the far lane, the row sits on the far side). Today these never meet because the coverages are staggered: lane j+1's doubled stretch lies beyond lane j's coverage. Overlapping the coverage with the bridge puts the row and the next lane's copy on the same line over the same stretch: A3's IIS (prio:B1#1 vs B2''#1).
- So the freed main-line-over form needs one of: (a) the row gapped alongside the neighbouring lane's doubled stretch too (joins either side; G9b decides whether the prio still holds); (b) lanes three lines apart in the join (row line and copy line separate: wider, still shorter); (c) the inner lane's copy on the joiner side instead of toward the far lane. Each changes how the join looks: an engineer call, by eye. Probes paused here.
