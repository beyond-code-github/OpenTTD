# Probe: why the W-port fan of LL2 o49t0f1s0 climbs 16 half tiles (area 1654.25)

Method: tagged every model row; took the final separations; added one probe row; re-solved; deletion-filtered the LP-IIS of the relaxation to a minimal core. Patch: ll2-fan-iis.patch (env PROBE=fan|porty FANCAP=n FANSTRAND=EF2 PLANAR_ONLY=o49t0f1s0). y grows south, half tiles.

## Results (EF1 fan = heading-1 segment, original L=16, objective area 1654.25)
- cap EF1 fan 2: INFEASIBLE (LP). Trivial core, 9 rows: EF2.start.y = EF1.start.y + 2 (W port lanes 2 apart); E2 row >= E1 row + 4 (rule L909, the gap line between the two diverging legs); every fan piece >= 2. So EF1 fan >= EF2 fan + 2 >= 4.
- cap 4: INFEASIBLE (LP), core 59 rows (chain below).
- cap 8: INFEASIBLE only as a MIP (parity), LP feasible.
- cap 10: feasible, area 1738.25. cap 12: feasible, area 1708.25.
- cap EF2 fan 6: MIP-infeasible (LP feasible).
- Extra row y(EO1 end) >= y(EF1 start): feasible, area 1738.25.
So the climb is NOT forced at 16: the model can do 10 at +84 tiles of area. 16 is the objective's pick; the forced part is the lower bound (about 10) below.

## Chain forcing a large climb (cap-4 core; all VERIFIED from the core unless marked)
1. W port lines are pinned relative to each other: EF1.y = p, EF2 = p+2, WO2 = p+6, WO1 = p+8 (L1579 equalities).
2. WO1's step (WO1#1 diagonal, the bundle step switch) has L <= 16 (OPTMAX), so WO1.start.y <= p+24.
3. C1/C2a/C2b stack: WO1.start >= C2a + 2 (sep C1#4/C2a#0) , C2a >= C2b#0 + 2 (rule L824), and C2b#0 = WO2.start.y + CBj2#6.L + C2b#5.L + C2b#2.L (each diagonal >= 2, rule L681). This caps how far south WO2.start / B2'' can be: about p+12.
4. B2'' (a copy of B2, tied at L870) is the over line crossing D2#4; D2#4 must end north of B2'' (cross rows: D2#4.L >= b+1, b = D2#3.y - B2''.y).
5. Above that D2 has three more diagonal pieces (D2#6, #8, #10, each >= 2, L681) and then D2 is 2 north of D1b (sep D1b#0/D2#12), D1b 2 north of D1a#0 (L824), D1a has two diagonals (D1a#2, #5, each >= 2; #2 is 4 in the solution), and E1 must be 2 north of D1a#5 (sep D1a#5/E1#3, dir (0,-1)).
6. E1's row = EF1.y - fan (EF1#2 end, E1#0..2 straight). So fan >= the sum of: the D2 diagonals, D1 diagonals, the three 2-gaps, the B2'' crossing margin. The LP lower bound is about 6-8; with parity/integers the minimum is 10.
7. INFERRED: why 16 and not 10: with E1 higher the outline's top edge (and the E-side ports EO1, which sit at E1's y) rise, shrinking the box height; the area objective trades 84 tiles for the extra 6 half tiles of climb. The E1 climb of 8 tiles is therefore the optimiser's choice among a family ranging 10..16, not a forced chain. The y(EO1 end) >= y(EF1 start) row costs +84 tiles (feasible).
