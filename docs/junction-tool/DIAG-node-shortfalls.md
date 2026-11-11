# DIAG-node-shortfalls (dg1): what the node model lacks, measured (not committed)

Tree: 952af32ad3, Release, scratch instrumentation (reverted). Scratch tests `[dg1][.]` in src/tests/junction_game.cpp. Raw outputs: /tmp/claude-1000/dg1-q1.txt, dg1-q2.txt, dg1-q3.txt.
Premise corrections: GenerateJunctionNear tries 17 offsets (centre + 8 at distance 2 + 8 at distance 4), not 9. The hidden [.slow] blocked-spot test FAILS now (`REQUIRE(near.result.layout.has_value())`). The ridge test result matches the brief ((0,0) fails, (-2,-2) is the second offset tried and succeeds).

## Q1 Ridge at (0,0)
Ridge = working rows 9..12 (ml_gap 1 + 2 + 6 = 9): row 9 z0..1 inclined SE (up as y grows), rows 10-11 plateau z1, row 12 z0..1 inclined NW. Start offset (14,-1); state offset after the anchor shift is (14,0) for the start layout, (14,-1) after normalisation (working = layout + offset).

(0,0): ONE state, solved (1.4 ms), CheckJunctionRules clean (8 ms), rejected only by JunctionNodeHeightsOk. Two paths fail (the check was made to continue past the first failure):
- path 4 (role 2 -> role 5): rides row 8 (flat, z0), then piece 17 at working 44,9 = layout (30,10), td2 = UPPER_E half piece, on row 9 (z0..1, incline SE): "not usable at height 0". UsableAt needs plane == z_max (1) for a slope; the path is still at 0. A path only climbs on an axis piece inclined along its direction (a ramp), never on a half piece.
- path 6 (role 4 -> role 3): enters from layout y 13 (working 12) on a Y piece (ramp, z0 -> 1), runs the plateau, climbs the S-bend stair 'cd' (halves at working 10,9,9,8 rows) and its last half piece at working 38,8 (flat z0) is "not usable at height 1": it must step down 1 and only a Y axis piece on row 9 can do it.
Rows on the ridge (layout y = working y + 1): row 9 carries the stair/diagonal halves 'cd' (x24,25), Y legs '|' x26, x28, halves 'ba' (x29,30); row 10 'c+.+a' (halves + switches); rows 11-12 only the two Y legs. No bridge heads or switches on the ridge rows themselves apart from the '+' at row 10 (x26, x28); the bridge head B at working row 8 sits one row above the ramp row.
At (-2,-2) the same four ridge rows land on layout rows 11-14 = the two straight Y legs only ('|.|'), where every piece is a ramp-capable axis piece. That is why only (-2,-2) works.

What the model lacks (measured facts + code read):
1. Heights are not in the solve at all. The solve checks each tile alone (Straight/Switch/Half "at z_max" facts); z-connectivity along a path (ramp up/down on an axis piece, half pieces only at the top) is only checked after the solve by JunctionNodeHeightsOk, and a failure there is not repaired (the search only creates children for Straight/Break failures).
2. The Y variables (node rows, stair start rows) are snapped to the flat values with no facts (DESIGN-N3 §5), so the ridge cannot move the stairs off the ramp row. Needed at (0,0): the transition from z0 to z1 must be a straight Y piece on working row 9 (and the way down likewise), i.e. the Y leg x26 must extend through row 9 and the stair starts at row <= 8 (path 6), and path 4's diagonal must not enter row 9 until after a Y ramp piece. In model terms: a Y-fact (a row where a half/curve piece may not sit), or a stair/diagonal start row that moves, not a new piece kind.
3. INFERRED: the old T map handled this because ConstructJunctionTMap grows the box height one row per failed height (heights_tried sweep, line router with ramps), moving the whole row structure relative to the ridge; the node model has a fixed topology height.

## Q2 The 6x6 block (hidden test)
Block: working x 37..42, y 2..7 = layout x 23..28, rows 2..7 at (0,0). Result per offset (all 17 tried, none accepted, each "1 state"):
- 16 of 17: solve failure kind Piece (1), with no link/column, so SearchJunctionNodes makes no detour child and the search ends at once. Class 2 (the N leg x25 rows 2-6 + its rigid S-bend to (28,8) + the S leg x28 rows 8-12, anchored by the S port, D178): offsets (0,0) (-2,-2) (0,-2) (2,-2) (-2,0) (2,0) (-2,2) (-4,-4) (0,-4) (-4,0) = 10. Class 1 ((22,0),(23,0): the row-0 switch pair at the N/E node): (0,2) (2,2) (0,4) (4,4) = 4. Class 10 (the W line's switch/bridge-head points (22,3)..(32,2) with the bridge (24,3)-(30,3)): (4,0) (-4,4) = 2.
- 1 of 17, (4,-4): the block covers only 7 free-link tiles (0 points, 0 rigid), the solve SUCCEEDS and the layout is then rejected after the checker by "fits: tile 35,6" and "heights: path 6 piece 5 at 38,9". Cause (code read, confirmed by this run): SolveJunctionNodes builds `diagonal_rasters` (junction_nodes.cpp:1022-1024) and never uses it, so free-diagonal interior tiles have no facts. (A free diagonal's stair tiles on blocked ground pass the solve.)
So by block-hit category: rigid piece (point or rigid-run) 16, free link only 1, other (port approach blocked) 0.

(0,0) layout with the block overlaid ('@' track on a blocked tile, '#' blocked empty, first column working y):
```
   0 -----------------------+--------------+--
   1 .......................cd...b---------a..
   2 --d...................b@@@#@@B-+-+-------
   3 ..c---+---------------+@@@@@#.Baba.......
   4 ......c-------------d..##@@##..ba........
   5 ....................cd.##@@##..|.........
   6 .....................cd##@@##..|.........
   7 ......................c@##@@#..|.........
   8 .......................cd.Bcd.ba.........
   9 ........................cd|.|ba..........
  10 .........................c+.+a...........
  11 ..........................|.|............
  12 ..........................|.|............
```
(layout x 0..40; rows are layout y + 0.) The 16 track tiles under the block: POINTS (25,2) (25,3) (25,5) (25,6) (26,3) (26,4); rigid-run (23,3) (24,2) (26,6) (26,7) (27,2) (27,3) (27,7) (28,2); free-link interior (25,4) (23,7). Rigid pieces dominate: the N leg's column (class 2: legs + the rigid S-bend between them + the anchored S leg are ONE variable fixed by the anchor), and the two bridge spans/heads of rows 2-3.
Shortest structural change (INFERRED, not tried): the block is 6 columns wide and the N cluster (legs x25, bridge decks x23..30) is ~8 wide, so the cluster must move >= 6 columns; class 2 cannot because it is anchored and its S-bend (25,6)->(28,8) is a rigid link. Candidates, record both: (a) make the N leg's S-bend a stretchable link (a free diagonal of >= 3 halves, extra map steps move x and y together) so the N leg can move left of x23 / right of x28 while the S leg stays on the anchor, then the crossing rule moves the bridge heads (classes 10/11) with it; (b) lengthen X runs so the bridges and N leg shift as a node row, which needs the leg decoupled from the S leg (same change as (a)). A Y move (rows) alone cannot clear it: the block spans rows 2-7 of a 13-row box.

## Q3 Checker vs solve
Run through SearchJunctionNodes at the start offset, 16-state cap, all states logged:
- Flat at 100/120/140/160/180/196 km/h (prio 9/8/10/11/12/14), house-beside-hub, blocked-hub-tile: each ACCEPTED, 1 candidate each (house/hub: 2 states, the first a Straight failure), 0 checker rejections.
- 200 random terrains (seeds 1..200, 1-3 blocked rectangles 1x1..3x3 inside the layout box): 9 skipped because a port approach is blocked (D179; GenerateJunctionNear returns before searching). 199 runs in total, 66 accepted.
- 259 states: 170 solve failures (Piece 65, Straight 105), 89 solved. Of the 89: 23 rejected after the solve by heights/fits (15 both, 8 fits only), 66 candidates passed to the checker. CheckJunctionRules rejected 0 of 66 candidates: rule tally empty, no messages. Over this sample D193's goal already holds: the checker never rejected a solve that passed heights/fits.
- Time (Release): CheckJunctionRules 8.3-8.5 ms per call (mean over 89 solved layouts); one solve 0.1-0.2 ms when it fails early, 1.0-1.6 ms when it renders (mean over all states 1.06 ms). The checker is about 6-8x a rendering solve and is the largest part of a 1-state search (the hover budget is 100 ms).
- The real rejections are all post-solve fits/heights (23) and solve failures of kind Piece (no repair) - see Q1/Q2.
