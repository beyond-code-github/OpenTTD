# DIAG: house at (xI-1,0), first height (13) only

Constants: x_w=14 x_e=54 xI=40 xO=42 g=1 w_row=2 b_row=3 ys=10 yb=12; house at (39,0). A->S first piece x_s=36, n_to_a_min_x=52.
Legend: A = A piece, s = A->S piece (overwrites A where shared), G = N->A goal, H = house, w_row=2, b_row=3.
D172 window: lo=38 hi=39 nmax=10 window=7; blocks x<32 or x>45 on rows 2 and 3 (n_to_a_blocked = row2:[0..31],[46..83], row3 same).
N->A start (40,10 td9) ; goals: (52,1 td8) (54,0 td8)   (NOT empty).

WITH HOUSE (H at 39,0)
x:  45678901234567890123456   (x=34..56; xI=40, xO=42, x_w=14, x_e=54)
 0 AAssAH.............AG..
 1 ...ssAAAAAAAAAAAAAGA...
 2 ....ss.................  <- w_row
 3 .....s.................  <- b_row
 4 .....s.................

FLAT (no house)
x:  45678901234567890123456   (x=34..56; xI=40, xO=42, x_w=14, x_e=54)
 0 AAssAAAAAAAAAAAAAAGGG..
 1 ...ss..................
 2 ....ss.................  <- w_row
 3 .....s.................  <- b_row
 4 .....s.................
A (house): straight on row 0 x=14..37, then drops to row 1 at x=38 (td5 at (38,0), td4 at (38,1)), runs along row 1 x=39..52, rises at (53,1) td10 / (53,0) td11, ends (54,0).
A (flat): straight on row 0, x=14..54. Flat N->A goals: (52,0) (53,0) (54,0).
A->S is identical in both runs.

## Failing edge and cause
Edge: N->A (A and A->S both succeed at the first height). Category (4): relaxed reachable, still fails with the D172 window removed; no rule named by the router.
Not (1): goals exist (2). Not (2): relaxed reachability true with n_to_a_blocked. Not (3): route_line with EMPTY blocked also fails. Not (5): A routes (by detouring to row 1 around the house).
Observation (not a proven rule): the detour puts A on row 1 for x=38..53, so only two plain goals remain at x>=52: (52,1) and (54,0), in place of three on row 0.

Log lines:
PROBE N->A start: (40,10 td9)
PROBE N->A goals: (52,1 td8) (54,0 td8)
height 13: N->A failed
PROBE N->A request min=(1,0) max=(68,12); relaxed reachable with n_to_a_blocked: true
PROBE relaxed reachable with EMPTY blocked: true
PROBE route_line N->A with EMPTY blocked: fails
