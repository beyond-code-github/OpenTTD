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

## lattice flex (shape-3 probe)

Head 758d142ac6 on wt-e1. Hidden test `planar shape: TS2 with lattice flex` `[.][shapeprobe]` (Release `build-rel`). Probe flag: `lattice_flex = false` as a new last parameter of `PlacePlanarShapeFlex` (and a `lattice` parameter of the static `FreeOnePieceStretch`, default false); true turns off only the channel clause (`channel[p] == channel[q]`) of `FreeOnePieceStretch`; the one-structure and copy-span clauses and the beside-parent clause stay. Production callers are unchanged (flag off).

Both variants (Shape's base, flex-stripped, and the 45-degree variant) were taken flex-stripped, flex placed by `PlacePlanarShapeFlex(v, why, true, lattice)`, cells refined (3 dummies), drawn at the default spacing with the DrawnFixture options, `least_extra = 0`, 15 s cap.

| variant | lattice clause | flex pairs placed | status | ms | rows x columns | inserts used |
|---|---|---|---|---|---|---|
| base (90 degrees) | on (flex C) | 34 | Infeasible | 41 | 1667 x 391 | 0 |
| base (90 degrees) | off | 34 | Infeasible | 38 | 1667 x 391 | 0 |
| 45 degrees | on (flex C) | 34 | Infeasible | 37 | 1606 x 391 | 0 |
| 45 degrees | off | 34 | Infeasible | 37 | 1606 x 391 | 0 |

The lattice flag changes nothing here: the flex placed is identical with it on and off (same tracks, same positions, 34 pairs each), so in TS2 as flex-stripped the channel clause is not what withholds flex. No Optimal drawing, so no domain verdict and no used inserts.

Conflicts with flex offered (lattice off), first conflict of the solver:
- 90 degrees: 119 rows (piece rows of AB1, AB2, AB3, A2, A3, B2, B3; `port N gap A2/A3`, `port E gap B3`, `port W gap AB2/AB3`; `node 1`; `cross 0`, `cross 2`; `curve A2`; `end 0`; `land 1`; many `clear` and `room` rows of cells 1, 2, 6). Not filtered (budget).
- 45 degrees: 39 rows (`piece` A2 x5, A3 x3, B1 0; `port N gap A2/A3`, `port E gap B2`, `port W gap AB2`; `cross 0-2`; `end 0`; `land 0`; `clear`/`room` rows of cells 5 and 8). Deletion filter stopped at the 30-solve cap after 29 of its 39 rows had been tried: skipping `port N gap A2` alone, or `port N gap A3` alone, turns it Optimal (244 and 263 ms); every other row skipped stayed Infeasible. The 13 rows left in the list (`port N gap A2`, `port N gap A3`, `clear 5 3 9`, `clear 5 3 16`, `clear 5 5 2`, `clear 5 6 2`, `clear 5 8 2`, `room 5 8 2`, `clear 5 8 10`, `clear 5 8 12`, `clear 5 8 14`, `clear 8 0 8`, `clear 8 0 10`) are not a proved minimal set.

Reading:
- Offering S-bends on every stretch does not make TS2 drawable at the default spacing at 90 or at 45 degrees, and at these two shapes the lattice clause is not the limit (the placement is the same).
- At 45 degrees the N gaps of A2 and A3 are each individually sufficient to remove (as in shape-2: the one-tile N port gap is the binding row); whether a wider N gap alone makes it Optimal with flex is not tried.
