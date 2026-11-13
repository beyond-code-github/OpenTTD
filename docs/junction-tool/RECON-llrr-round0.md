# RECON: LL_RR x LL_RR round 0 infeasible (daec34b101, all-floor choices)

Method: throwaway test + stderr dump of the infeasible rows (reverted). Loop round 0 and a direct SolveJunctionJoint on the all-floor recording give the SAME 7-row set (the "Corner size (edge 3) <- ... bridge over S-bend (edge 5)" chain in the brief is a longer variant; this build names the short one below). The T bound row (t <= t0 + t1 L) and t >= 1 are bounds, not relations, so the conflict list omits them, but they are in the set.

## 1. The conflict (in order), axis x throughout, all relations "x(v) - x(u) >= c" (a=0 unless stated)
| # | reason | v | u | c | element of v / u |
|---|---|---|---|---|---|
| R1 | bridge heads (edge 14) | 80 | 81 | 0 | edge 14 Heads first/last: same column |
| R2 | bridge over S-bend (edge 14) | 84 | 80 | -1, over d=-1 (t rec. 1) | x84 - x80 + t >= 0, i.e. x80 <= x84 + t; pt 84 = start of edge 17 SBend tok 4 (bend 19) |
| R3 | S-bend size (edge 17) | 85 | 84 | a=1 L19 | x85 >= x84 + L19; bend 19: edge 17 tok 4, hand -1, hin 2 (south), steps EAST |
| R4 | lane split run (edge 29, node 10, row 3) | 192 | 176 | 1 | copy-1 pair (191,192) last vs copy-0 pair (175,176) last: head stagger |
| R5 | lane split run (same) | 176 | 177 | 2 | pair b=1 (175,176) vs pair b=2 (177,178): head run floor |
| R6 | bridge heads hug crossers (edge 29, row 3) | 81 | 192 | 1 | crosser edge 14 (pt 81) inside bridge b=1 of edge 29 (crosses edges 15,14); 192 = its far head |
| R7 | bridge heads hug crossers (same) | 177 | 85 | 1 | near head of bridge b=2 of edge 29 (crosses 17,16) vs crosser edge 17 at pt 85 |
Edge 29 heads west (hin 3), doubled, copies 0/1, series bridges b0 (31,30), b1 (15,14), b2 (17,16), b3 (8,5), b4 (22,20).

Cycle (x, west is smaller):
  x80 >= x81 >= x192+1 >= x176+2 >= x177+4 >= x85+5 >= x84+L19+5 >= (x80 - t) + L19 + 5
  => t >= L19 + 5.   Bound row: t <= t0 + t1 L19 = L19 - 1 (t0=-1, t1=1).
  => 0 >= 6 for every L19 and t. Positive constant 6, independent of L and t: no length or column helps.
(5 = hug 1 + run floor 2 + stagger 1 + hug 1, each a rule minimum.)

## 2. Geometry
- Bridge: edge 14, single (BRIDGE edge 14 crossed [17]), Over token tok 7, heading south, its column x80=x81. Heads are the "Heads" element of edge 14 (pts 80,81).
- Crossed edge 17 program: tok 4 SBend(-1), tok 5 Under(14), tok 6 SBend(-1). Both S-bends step east (bend 19 L19, pts 84->85; bend 20 L20, pts 86->36). Under(14) sits BETWEEN them.
- The model attaches the bridge to bend 19 only: the code takes the S-bend whose NEXT token is the Under (o.tok + 1 == Under). Per DESIGN-G1 10.7 "SBend then Under(x)" that S-bend is the one before the Under token.
- Why it cannot sit on bend 19's diagonal: edge 29's series doubled bridges cross edge 14 on b1 and edge 17 on b2, b1 east of b2 (west-bound). Hug + run floor + stagger force edge 14's column >= x85 + 5, i.e. 5 columns east of the END of bend 19. Bend 19's diagonal covers columns x84+1..x85-1 only. It is the other bridge's (edge 29) series heads, not hug alone: hug R6/R7 plus lane split runs R4/R5 together. Drop any of them and it is feasible (section 4 for hug).
- On the straight run before/after: the run after bend 19 is column x85 = x86 (x85 .. x86 is 3 rows in the drop-over solution), also < x85+5, so no. The only place that fits is BEND 20's diagonal (the S-bend after the Under token). Evidence: with the over rows dropped the joint solution puts edge 14 at x80=75, y80=55; x84=59, x85=61, x86=61, L19=2, L20=15, x36=76: x80 = x86 + 14 = x86 + (L20 - 1), the last diagonal column of bend 20. So the crossing is geometrically fine over bend 20; the model has no t on bend 20.
- Caveat (not tested): hug R7 ties edge 17's crossed tile to pt 85 with dx=0, frozen from the all-floor recording; at the drop-over solution edge 17 at edge 29's row (y47) is on bend 20 (x about 65), so the hug set is stale there and a later round would re-record it. This is the loop's job once round 0 is feasible.

## 3. Which rule
- Code: junction_placement.cpp:1806 comment cites "D113, F4, DESIGN-G3 G3.4a". D113 (no split in a bridge's block) is not about this; I found no D-id that decides "bridge over the S-bend of the crossed edge".
- Origin: DESIGN-G1.md 10.7 (line 124) "Under x's single bridge (a band crossing, F2): SBend then Under(x)" and finding F2 (line 112): "a band crossing is made by an S-bend on the crossed edge under the joiner's single bridge (the T's D2 under N->E); a rule, not read off the trace". Single bridge = D174 (branch bridges single). So Over/Under pairing with the S-bend before the Under token is a G1 program rule (F2), designed for the T's one-S-bend case; nothing says the S-bend after the Under is excluded or that two S-bends can flank an Under.
- DESIGN-G3-vectors.md G3.4a (line 78) generalised t over "the bend" and says "today a single bridge over an S-bend stands in the column next to the start (t=1)"; premise P3 diagonal columns T(L) = T0 + T1 L.

## 4. Variants (JT_DROP throwaway filter on the reason text, round 0 all-floor)
- Drop "bridge heads hug crossers": FEASIBLE (Optimal). x80=39=x84+1 (t=1), L19=2, L20=1; no t moves except unrelated overs (edge 5 tok 6 +6 ... = 7, edge 8, edge 47). So the cycle needs the hug rows (R6, R7); it is not hug-only: R4, R5 and R3 are in it.
- Drop "bridge over S-bend": FEASIBLE. t is then x80 - x84 = 16 with L19 = 2 (beyond the diagonal, the affine continuation), geometrically x80 = x86 + (L20 - 1): bend 20's last diagonal column.
So the cycle is hug + over + lane split runs. Removing either hug or over alone makes round 0 feasible; neither is a mere artefact.

## Suggested fix direction (not done)
Let a single bridge over an Under token pick, per crossing, the flanking S-bend (before, tok-1, or after, tok+1) as a model choice; or put t on the flanking pair as one diagonal of length L19 + run + L20. Each choice adds a T per candidate S-bend with a binary pick (plain model first, per simplicity). Check D-ruling: none found, so would be an orchestrator default per D227.
