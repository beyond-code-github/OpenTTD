# RECON FX1 loops: why each base IIS cannot close (probe-fx1 387f0ac913; not committed)

Sources: scratchpad/fx1/iis-{T2-o0t0f1s0,T3-o0t0f0s0,LL2-o0t0f0s0}.txt (first block to "port order"). Conventions VERIFIED from source: headings STEP = N,NE,E,SE,S,SW,W,NW with y down (planar_model.h:45); a seg row is end - start - L*STEP = 0, so a coefficient +1 on L in a y row means dy = -1 per half-tile, -1 means dy = +1 (coefficient 0 = no y change); a diagonal's least length is 1, an axis piece's 2 (AddPiece, planar_model.h:432); a slot-bend diagonal is cut into a piece at exactly 2|d| (planar_components.cpp, add_part, `AddPiece(nm, Turn(..), len, len)`); port order row: consecutive ports on a side 4 apart, lanes of one port 2 apart (planar_components.cpp:1313). IIS rows print no variable bounds; every L below carries its least length from the rules above. Which piece is bend / site leg / axis is INFERRED from names and row shape (no heading dump exists for these variants; the G3E_DUMP step is hard-wired to T2 o1t3f1s0).

## 1. T2 o0t0f1s0 (sideways = y, E side)
Rows (VERIFIED): seg AB2#0-2, B2#0, B2"8#0, D2#1-4, ED2#0-2; tie D2#4.end = E2#5.end = ED2.start; cross B2"8#1>D2#1 (L1 - v >= 1; B2"8#0.end.y - D2#0.end.y + v = 0); port order AB2.start.y - ED2#2.end.y = 4.
Topology: AB2 = in-port lane 2 on E (heading W); B2/B2"8 = its leg through the A/B split site; D2 = from S port (heading N) to the E2+D2 join; ED2 = out-port lane 2 on E. B2"8 (W-going) crosses over D2 (N-going); ED port is the northern neighbour of the AB port.
Loop (start AB2.start, y = a):
1. AB2#0 axis, fixed 0. AB2#1 diagonal (bend, dy -1, length La). AB2#2 axis, 0. Node A2/B2 start y = a - La. [(a) one channel; bend INFERRED]
2. B2#0 axis, 0 (across the split site). B2"8#0 diagonal, dy -1, length Lb free >= 1. Crossing level y_c = a - La - Lb. [(b) site leg]
3. Cross row: D2#1 (N run, under) must reach 1 past y_c: L1 >= 1 + yD - y_c, yD = D2#0.end.y (free). [(a), crossing inside D2's channel]
4. D2#1 -L1, D2#2 -L2 (diagonal), D2#3 axis 0, D2#4 -L4 (join-site leg diagonal): tie to ED2.start. [D2#1,#2 (a); D2#4 and tie (b)]
5. ED2#0 axis 0, ED2#1 -Le (diagonal/bend), ED2#2 axis 0, port. Pin: a - (yD - L1 - L2 - L4 - Le) = 4. [ED2#1 (a) or (b) INFERRED; ED2#2 and AB2#0 stubs (c): contribute 0, no port step]
Why it cannot close (derived from the rows): substitute the pin and y_c into the cross row; L1 cancels: L2 + L4 + Le + La + Lb <= 3. All five are diagonals/bends with L >= 1 (bends >= 2), so the left side is >= 5. Every free term has a fixed sign (all dy = -1, i.e. toward the north); nothing can subtract. The only slack is the 4 between the AB and ED port rows, and the crossing needs D2 to be above B2"8 by >= 1 while D2's own steps (L2,L4,Le) and B2"8's (La,Lb) all consume it. Fixed parts: none (all RHS 0); everything is free-and-one-signed. VERIFIED algebra, INFERRED labels.

## 2. T3 o0t0f0s0 (sideways = x, S side)
Rows (VERIFIED): seg CD3#0, D3#0, F3#1-4, AF3#0; tie F3#4.end = A3#3.end = AF3.start; cross D3#1>F3#1 (F3#1.L - v >= 1; D3#0.end.x - F3#0.end.x - v = 0); port order AF3#0.end.x - CD3.start.x = 4.
Topology: CD3 = in-port lane 3 on S (heading N); D3 = its leg at the C/D split site, turning east, going over F3; F3 = from W port heading E then S; A3/F3 join at AF; AF3 = out-port on S, 4 east of the CD port.
Loop (start CD3.start, x = c):
1. CD3#0 axis N, 0; node C3/D3 start x = c. [(c) port corridor, then site]
2. D3#0 diagonal NE, dx +Ld (free, >= 1). D3#1 axis N, 0, crossing at x_c = c + Ld. [(b) site leg; crossing (a)]
3. Cross row: F3#1 (E run, under) reaches >= 1 past x_c: L1 >= 1 + x_c - xF, xF = F3#0.end.x. [(a)]
4. F3#1 +L1, F3#2 +L2 (diagonal), F3#3 axis 0, F3#4 +L4 (join leg diagonal) tie to AF3.start. [F3#1,#2 (a); F3#4 (b)]
5. AF3#0 axis, 0, port. Pin: xF + L1 + L2 + L4 = c + 4. [(c)]
Why it cannot close: eliminating L1: Ld + L2 + L4 <= 3. Three diagonals at the rule minimum 1 give exactly 3, so the rows alone leave a zero-slack point; the IIS is therefore infeasible only because at least one variable bound exceeds 1 (not printed): INFERRED, e.g. a bend at 2|d| >= 2 or the D3#0.L >= 2 "deck-neighbours" row seen in the o0t0f0s0 second listing (PROBE-FX1.md). All signs fixed (dx +1 on every L). This sample is the tightest of the three.

## 3. LL2 o0t0f0s0 (sideways = y, W side; no cross, no deck)
Rows (VERIFIED): seg B1a#0-3, B1b#0-1, CBj1#0-1, CBj2#0-1, WO1#0, WO2#0; ties B1a#3=C2a#2=CBj1.start, B1b#1=C2b#2=CBj2.start, CBj1#1=C1#2=WO1.start; port order WO1#0.end.y - WO2#0.end.y = 2 (WO1 south, same port, lane gap 2).
Topology: B1 splits (node B1 -> B1a,B1b) at yB; B1a goes via bridge CBj1 to WO1; B1b via CBj2 to WO2; W heading.
Loop (start WO2, y = w; port end -> back):
1. WO2#0 axis 0; CBj2#1 diagonal NW, dy -Lc2 (free >= 1) so CBj2 start = w + Lc2. [(c) stub; (b) join node CBj2,B2 -> WO2]
2. CBj2#0 axis 0; tie to B1b#1 axis 0; B1b#0 diagonal NW, dy -Lb0: yB = w + Lc2 + Lb0. [(b) tie and legs; B1b#0 at the B1 split (b)]
3. B1a#0 diagonal SW, dy +La0; B1a#1 axis; B1a#2 diagonal SW, +La2 (bend, >= 2); B1a#3 axis; tie to CBj1.start. [B1a#0 (b); B1a#2 (a); tie (b)]
4. CBj1#0 axis; CBj1#1 diagonal SW +Lc1; tie to WO1.start; WO1#0 axis, port. [(b), (c)]
Pin: (yB + La0 + La2 + Lc1) - w = 2.
Why it cannot close: La0 + La2 + Lc1 + Lb0 + Lc2 = 2, five diagonal terms each >= 1 (La2 >= 2), so >= 5 > 2. B1a's three steps all go south, B1b's two go north (signs forced by the rows); nothing is fixed, nothing can cancel. VERIFIED algebra from rows.

## Common
Each loop closes a port-order pin (2 or 4) against the sum of one-signed minimum diagonal steps along two routes that diverge at a site or cross; steps (c) contribute 0 here (axis stubs, no port step). INFERRED classification: (a) inside-channel bends and crossing runs, (b) site legs and ties are the bulk of the sum.
