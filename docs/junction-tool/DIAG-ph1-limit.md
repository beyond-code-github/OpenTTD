# DIAG ph1-limit: N->B fails on the "W end height change" terrain (height 13)

Worktree .claude/diag2 (wt-diag2), Release, temporary prints, all reverted (git status clean).

## 1. N->B's request at height 13 (identical on flat and raised terrain)
- Frame: xI=40 xO=42 x_w=14 x_e=54 yb=12 ys=10 w_row=2 b_row=3. STUB=2.
- Skeleton (stagger 1): x_sl=47 x_ls=45 x_he=43,44 x_hw=37,38 x_lm=35 x_nw=20 x_sb=16; crossing zone [38,42], zone_b [39,41].
  Rules (junction_skeleton.cpp): x_lm - x_nw >= cover + k_w (15), x_nw pushed Up, so x_nw=20 is the maximum the rule allows; x_sb = x_nw - 4.
- Start: (40,11) Y_NW (td9). Goals = PlainRowPieces(b_row, X_NE, x_sb+1, x_nw): (17,3) (18,3) (19,3) (20,3), all td0.
- Bounds 1,0..68,12, plane 0. Blocked: the crossing window (38..42,2) (39..43,3), row 1 whole, row 12 except the stubs.
- Existing in layout: A (row 0), S/N port stubs, lane A/B rows 1-3 incl. bridges (37,2)->(43,2), (38,3)->(44,3), lane B's X track on row 3 at x=17..35, its W-end curve (16,3)/(16,2), B->S edge.
- Terrain: x<=18 at z1, x=19 the 0->1 incline column (x_w+STUB+3), x>=20 z0. The goal window 17..20 straddles the incline: 17,18 on z1, 19 on the ramp, 20 on z0.
- Start usable, z 0->0. The router expands z0 over x 20..68, ramps up on x=19 (straight X pieces only) and reaches z1 on x 1..18.

## 2. Why N->B finds no route
Not the relaxation skip (relaxed=1), not the expansion limit: the search EXHAUSTS (39331 expansions, limit 65625).
Flat control: 817 expansions, route RFFFFFFFFFFFRFFFFFFFFFFFFLFR to goal (19,3).
Counts in the raised run: bounds/blocked 9890, height 1357, existing-track 8031, occupy 3786, curve 29636, CS2 goal refusals 2.

How a goal is entered: an X_NE goal piece (x,3) on existing X track is reached by the S-jog LEFT_N (td12) at (x+1,4), RIGHT_N (td13) at (x+1,3), then X_NE at (x,3).
The td13 piece clashes with the existing X on (x+1,3), allowed only on a goal tile (the old router's exception 2), so x+1 must be a goal tile and a flat/usable tile.
Each goal, in the raised terrain:
- (20,3): needs the td13 piece on (21,3), not a goal tile: refused "existing" (also refused on the flat terrain; it is just never the cheapest there). 
- (19,3): td12/td13 on x=20 are fine at z0, but (19,3) is a ramp piece, and the router lets a ramp piece come only from an F move (allow_ramp = move=='F'); here it is entered by the R move: refused "height".
- (18,3): needs td12 at (19,4), a curve piece on the incline column: refused "height" (HeightStep: a non-straight piece on an inclined tile is UsableAt only at flat heights). Same for any curve on x=19.
- (17,3): reachable (td12 (18,4) z1, td13 (18,3), X_NE (17,3) at z1) and popped once ("goal piece reached 17,3 z1 goal=0"), but IsGoal refuses it by CS2: the existing continuation (16,3) is the W-end curve right after the goal, so the curve spacing/S-bend rule fails (2 refusals).
So height refuses 18 and 19, the "existing" obstacle rule refuses 20, CS2 refuses 17. Terrain z of the goals matters only through the ramp column: the z reached at the goal tiles is right (z0 at 19/20, z1 at 17/18).

## 3. Same edge on the flat terrain
Same request. Route RFFFFFFFFFFFRFFFFFFFFFFFFLFR, cost-found goal (19,3), 817 expansions, z0 throughout; final S-jog td12 (20,4), td13 (20,3), X_NE (19,3).
Goal (19,3) is the only one it needs; (20,3) is unreachable there too, and 17/18 are not tried before the cheaper 19 is popped.

## Experiment: move the goal window
Temporary +1 on the goal window's upper end (goals 17..21, i.e. x_nw = 21): N->B routes at height 13 (route ...LFR to goal (20,3) at z0, 755 expansions) and the map is "built" at height 13.
(20,3) works because (21,3) becomes a goal tile (td13 clash allowed) and the X_NE piece (20,3) is flat; its continuation (19 ramp, 18, 17, 16 curve) passes CS2 (4 pieces from the curve).

## Conclusion
Cause: the goal window 17..20 sits across the incline column x=19: the router refuses a ramp piece entered by a turn move (goal 19), a curve on the incline (goal 18), and CS2 refuses goal 17 (existing curve at 16 right behind it); goal 20 was never available because its S-jog curve would sit on (21,3), outside the goal window.
Skeleton: x_nw would have to be 21 (+1; x_sb 17, and x_lm 36, which drags the crossing east by 1) for N->B to route here. That is not a free choice: x_nw=20 is already the PrioCoverage maximum, so +1 breaks the rule rather than refines the skeleton. The cause is in the router/terrain interplay (ramp pieces and goal-tile exceptions), not a mispositioned skeleton.
