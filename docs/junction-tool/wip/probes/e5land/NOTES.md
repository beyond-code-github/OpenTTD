# E5 landings probe (PSG 197, clearance rows off, one face's dummies at a time)
Method: temporary env PLANAR_DRAW_FACE=f (landing rows for that face's dummies only) and PLANAR_DRAW_DROP (dummies left out), both reverted.
Headings: 0 = north, 2 = east, 4 = south, 6 = west (STEP). Points: point[k] = start of piece k; a dummy starts at the END of its named piece.
Faces with dummies: 162. Tested largest 104 (n 14 down to 4; 58 smaller faces, n<=4, not run: budget). Each solve cap 15 s.
Result: feasible 94, Infeasible (proven) 2 (faces 16, 17), 15 s limit "Error" (undecided) 7 (faces 155, 156, 71, 79, 61, 110, 97).
Face 16 (5 dummies) minimal infeasible set = {dummy 153}: start SO1 end of piece 0 (SO1 p0 h5 SW), heading north, lands on A2c piece 10 (h3 SE). Kind: on a track.
Face 17 (6 dummies) minimal infeasible set = {dummy 158}: start SO2 end of piece 3 (p3 h5 SW), heading north, lands on SO2 piece 1 (h3 SE), the same track's earlier piece. Kind: on a track.
Reason (face 17, exact): SO2 runs p1 SE (l1), p2 S, p3 SW (l3); the vertex x is x(p1) + l1 - l3; the north ray lands on p1 only if 0 <= l1 - l3 (<= l1). So the track must end no further west than the start of its SE piece; the rest of the program (port order on the south side, node x order) puts SO2's port 7 half-tiles west of its node (l3 - l1 = 7 in the dummy-free solution). A hook (SE out then SW back) whose north ray must hit the outgoing leg needs the net x of the hook >= 0, i.e. the opposite of the hook's net turn.
Face 16 same shape: SO1 p0 SW leaves node (x_n, y_n) to x_n - l; the ray north must meet A2c p10 (SE, x increasing) at x_n - l, so A2c.10 must start west of SO1's port x; the dummy-free solution has A2c.10 at length 0 at the node x, and the S port order forces SO1's port x west of it only by the SW leg, with A2c.10 needing length >= its own offset.
IIS of the single-dummy program names the global rows (port S, node, cross, piece rows), so it is the lengths/order of the shape, not a local clash.
Landing kinds in the 2 proven sets: 2 on a track, 0 on a dummy, 0 on the box. (Faces 17's full set also has landings on dummy 160, box 2, SO3[1], D123[12]; they are not in the minimal set.)
Counts of landing kinds over the 7 undecided faces: 155 has 1 on a dummy, 61 has 1 on a dummy, the others track only.
