# Probe: ladders from a rough solve (2026-11-17, HEAD 05e0159b09), fixed shapes

Result: the ladder is feasible and costs nothing, but removes lazy rounds only where the rounds are between lines that share a corridor. Area unchanged on all four. T2 and T3 keep the same clashes (C vs F lines), which no rung covers. LL² loses its one clash round.
Code: wip/probe-ladders.patch (on top of probe-corridors.patch's face/wall code; env PLANAR_LADDER=<mode>, PLANAR_CLASHES=1 lists clashes). Raw output in the scratchpad `ladders/`.

## Construction
- Rung constant: SepOptions uses `>= 2` for every direction f (no per-heading scale), so rows are `Dot(Right(h), R_j.a) - Dot(Right(h), p) >= 2` (and the mirror for R points).
- Wall segments from the corridor code; a stretch whose end crossing's segment precedes its start's is skipped (T2 2, T3 4, LL² o49 4, o1t3 0). Rotating stretches into chain order changes only the list order, not the set, so it was not needed here.
- Brief-literal rows (every end point) do NOT all hold on S: T2 25 of 346, T3 69/743, LL² o49 21/596, o1t3 22/545 are violated, and the model is then infeasible ("base infeasible"). Cause: the segment holding a crossing continues past the wall; its far end (first segment's a, last segment's b of a stretch that starts/ends at a crossing) is not on the wall. Dropping exactly those points (call it "off-wall") leaves 0 violated on S. All runs below use that (mode 3). This is a reading of "shared vertex s,t", wider than the brief's wording (it also drops the interior diamond joints).

## Results (rough = today's solve at HEAD; ladder = rows added up front, lazy loop still on)
| variant | rows (shared dropped, off-wall dropped, no span) | rough: area, ms, clashes per lazy round | ladder: area, ms, clashes per round |
| T2 o0t2f1s1 | 217 (18, 147, 0) | 906.25, 135, 8 8 3 3 0 then 0 | 906.25, 148, 8 8 3 3 0 then 0 |
| T3 o0t1f1s1 | 404 (25, 382, 1) | 1941.25, 191, 0 then 8 3 0 | 1941.25, 206, 0 then 8 3 0 |
| LL² o49t0f1s0 | 470 (75, 147, 2) | 1654.25, 1846, 18 0 then 0 | 1654.25, 1806, 0 then 0 |
| LL² o1t3f0s0 | 407 (76, 146, 3) | 1992.25, 1370, 21 0 then 0 | 1992.25, 853, 0 then 0 |
("then" = the second lazy solve, after the area linearisation. Round-0 clashes are those of the first solve.)
- Round-0 clashes of the first solve: T2 8, T3 0, LL² 18 and 21 in rough; ladder T2 8, T3 0, LL² 0 and 0.
- Surviving pairs (T2 round 0, same pairs in T3's second solve; base strands): C1#0/#2/#3 vs F2#4; C2#1/#3/#4 vs F1#4 and F2#4 (T3: C2, C3 vs F2, F3#4). No rung ties a C line to an F line, so they sit outside every corridor wall pair.
- Pass 3 (rebuild after a point moved off its span): T2 and T3 none moved. LL² o49 and o1t3 each moved 2 rung points in pass 2; pass 3 gives the same area, the same times (1802, 854 ms) and 0 clashes in every round.
- LL² o1t3's ms drop (1370 to 853) is the one clash round gone; one run each, not repeated.
- No infeasible run with the off-wall rule. Infeasible: the brief-literal rows (above); no IIS taken (cause known from the S check).

## Not tested
Free bends; the C/F clashes' own corridor (faces with a box edge carry no rungs); repeat timings.
