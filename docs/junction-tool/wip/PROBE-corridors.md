# Probe: corridor walls (R1 phase matching), 2026-11-17, HEAD 05e0159b09

Result: Phase A FAILS. R1 is violated on every solved layout tested; phase B (PLANAR_CORRIDOR knob) was not measured, per the brief.
Probe code: wip/probe-corridors.patch (corridors in PlanarLineSides, R1 rows and PLANAR_CORRIDORCHK in MappedComposer; PLANAR_CORRIDOR=1 would add the rows).

## Phase A (verified by running, rows = point-pair rows, 4 per segment pair; rows whose two points are identical are dropped)
| hub variant | faces used | pairs | rows | skipped | violated rows (segment pairs) |
| T2 o0t2f1s1 | 17 | 68 | 552 | 2 | 16 (4) |
| T3 o0t1f1s1 | 43 | 141 | 1100 | 9 | 24 (6) |
| LL2 o49t0f1s0 | 22 | 91 | 1104 | 10 | 20 (5) |
| LL2 o1t3f0s0 | 24 | 87 | 1180 | 5 | 4 (1) |
Faces used = faces with no box edge and two non-empty walls (T2 has 32 faces in all, so the rest are box faces, the triangle or unusable).
Violations, T2: face 14 (L=A2#1, R=B1#1 F1#5) h6 A2#1 vs B1#6..8 slack -2; face 20 (L=C2#1, R=B1#5 D1#1) h7 C2#3 vs B1#11 slack -26.
T3: face 24 (A3#2 | B1#2 F1#8) h6 slack -2; face 34 (C3#2 | B1#8 D1#2) h7 slack -12 and -22; face 35 (C3#3 B1#8 | D1#3 B2#7) h7 B1#11 vs B2#11 slack -2.
LL2 o49: faces 20, 21, 30 (slack -6 to -26); LL2 o1t3f0s0: face 15, D1b#4 vs D2#2 h1 slack -20.

## Why (inferred from the violated rows)
- The "single run against every run of h on the other side" arm pairs a run with runs far along the corridor: B1's later west runs (B1#6..8, #11) are not alongside A2#1 / C2#3, they lie across a turn where the corridor has turned and the walls differ in position along the travel axis; the right-wall point then lies on the wrong side of the left run's line.
- Rows with nL == nR also fail when the runs are not at the same phase (face 35, face 15): equal counts do not mean the k-th runs are alongside.
- Bounded slack -2 cases (T2 face 14, T3 face 24): a run the other wall passes just before it starts, only a run's own extent is alongside.

## Matching and data gaps (verified)
- Skipped (h, face) (nL != nR, neither 1): T2 1; T3 7; LL2 o49 8, o1t3f0s0 5 (face lists in the run output).
- "Unusable" faces (T2 1, T3 2, LL2 o49 2): a stretch whose end crossing's segment index is before its start's (T2 F1#3 segs 4..3; T3 F1#5, F2#4): the composer's segment order does not follow the topology's event order on F, so crossing records cannot bound the stretch by event order.
- A crossing was always matched to one CrossRec by strand pair (no multi-record notes appeared).

## Not tested
Phase B (rows up front, timings, areas, lazy rounds), LL2 other variants.
