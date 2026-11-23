# TS2 at 45 degrees (shape-2 probe)

Head cfc20e73fa on wt-e1. Hidden test `planar shape: TS2 at 45 degrees` `[.][shapeprobe]` in `planar_domain_encoding.cpp` (Release `build-rel`; every solve 2-31 ms). TS2 = `tunnel T1 B1: A2, A3`, `tunnel T2 B2: A3`, straights `AB<k>: B<k>`, ports at the DEFAULT spacing (no `@`). Options: the DrawnFixture ones (corridor, end_node, cross_piece, node_lead, grid, tidy, integer), `least_extra = 0`, flex-stripped variant, `RefinePlanarShapeCells` before drawing.

## Step 1: Shape's TS2 variant (base)

As Shape gives it (with flex pairs): A2 `1 0 1 2 1 0 -1 0 1 0 -1 0 1 0`, crossing B1 on piece 9 (heading 0); A3 `1 0 1 2 1 0 -1 0 1 0 -1 0 1 0 -1 0 1 0`, crossings B2 on piece 9 and B1 on piece 13 (heading 0 each); B1 `2 1 2 3 2 1 2 3 2` (crossings on piece 4, heading 2); B2 likewise; AB and B3 `2 1 2 3 2`; A1 `1 0 1 2 1 0 -1 0 1 0`. Turning 3.
Flex-stripped (what is drawn): A1, A2, A3 `1 0`; AB and B straight `2`. A2 crosses B1 on its piece 1 (heading 0), A3 crosses B2 and B1 on its piece 1 (heading 0).
Crossing angles: A2 x B1 2 eighths (90 degrees), A3 x B1 2, A3 x B2 2. All three are right angles.

## Step 2: the 45-degree variant

Loaded with `LoadPlanarShapeVariant` from the shape text (the loader recomputes the angles from the headings, the flex-stripped form, no flex pairs):
```
shape AB1: 2 / AB2: 2 / AB3: 2 / A1: 1 0 / A2: 1 0 / A3: 1 0 / B1: 2 / B2: 2 / B3: 2
at A2: B1@0   at A3: B2@0, B1@0   at B1: A2@0, A3@0   at B2: A3@0
```
So A2 and A3 cross on their piece 0 (heading 1) and turn to 0 on piece 1, after their last crossing; each stretch is monotone; each diverging leg is one eighth off its trunk (A at 1 against B at 2). Turning 3, equal to the base. Angles: A2 x B1 1 eighth (45 degrees), A3 x B1 1, A3 x B2 1.
Checks: `CheckPlanarShapePromise` on the loaded variant: H12 (three "turns both ways" messages, one per A line, before cells are refined) clears once `RefinePlanarShapeCells` has run (3 dummies, as for the base); the only message left is H9 P6 on structures 0 and 1 ("no copy but its separator has a full share"), the same two messages the flex-stripped base gives, so nothing is refused for being at 45 degrees. Angles H4 clean.

## Step 3: drawing at the default spacing

| variant | status | ms | rows x columns |
|---|---|---|---|
| base (90 degrees) | Infeasible | 5 | 307 x 165 |
| 45 degrees | Infeasible | 5 | 301 x 165 |

No Optimal drawing, so no domain verdict. Both are also Infeasible with grid, end_node, cross_piece and node_lead off (3 ms, 4 ms).

## Step 4: the conflict of the 45-degree variant

Deletion filter (22 solves, one per row of the first conflict, a row kept dropped when the program stays Infeasible). First conflict, 21 rows: `piece A1 1`, `piece A2 0`, `piece A2 1`, `piece A3 0`, `piece A3 1`, `piece B1 0`, `piece B2 0`, `port N gap A2`, `port N gap A3`, `port E gap B2`, `port W gap AB2`, `node 0`, `node 1`, `cross 0`, `cross 1`, `cross 2`, `curve A2`, `curve A3`, `end 1 node`, `land 2`, `clear 5 0 4`.
Minimal conflict, 8 rows: `piece A3 1`, `port N gap A2`, `port N gap A3`, `node 1`, `cross 0`, `cross 1`, `cross 2`, `clear 5 0 4`. (The last solve of the filter had the other 13 rows skipped and stayed Infeasible; each of the 8 turned it Optimal when skipped in turn.)

## Reading

- At 45 degrees the W/E lane-spacing conflict of ts2-diag.md is gone (no `port E gap`, `port W gap`, `land` or `curve` row is in the minimal set), but the base is still Infeasible at the default spacing: the conflict moves to the N ports. It is now the three crossing rows (`cross 0-2`, A2 x B1, A3 x B1, A3 x B2 at 45 degrees) with the one-tile N gaps of A2 and A3, A3's last piece, and A3's node (`node 1`, `clear 5 0 4`). The rows name the conflict; why they cannot fit together (for instance the two diagonals one tile apart at the N ports crossing the separator at 45 degrees) is a reading of mine, not shown by this probe.
- So the angle alone does not make TS2 drawable at the default port spacing; whether a wider N gap (not tried, outside this brief) or a rule (`cross` or `clear 5 0 4` too strict) is the cause is the next question.
