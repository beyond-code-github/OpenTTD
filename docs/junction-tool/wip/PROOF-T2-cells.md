# T2 hand proof of SPEC-shape §8 (D385): faces at Shape-piece level

Variant o3t2f1m150 (T2 top). Source: probe env JUNCTION_G2_FACES=1, ng1 1a287ead1c. Walls walked with the face on the left; <t> is the corner turn in eighths (negative convex, positive reflex); the closing corner is computed (total -8).

```
FACE 0 [EF1 s0 p0 H2] <+0> [E1 s0 p0 H2] <+0> [prio:E1 s0 p0 H2] <+0> [ED1 s0 p0 H2] <-2> [box0 H0] <-2> [box-2 H-2] <-2> [box-4 H-4]
FACE 1 [EF2 s0 p0 H2] <+1> [EF2 s0 p1 H3] <+0> [E2 s0 p0 H3] <-3> [F1 s0 p1 H0] <-1> [F1 s0 p0 H-1] <-1> [EF1 s0 p0 H-2] <-2> [box-4 H-4]
FACE 2 [EF2 s0 p1 H7] <-1> [EF2 s0 p0 H6] <-2> [box4 H4] <-2> [CB2 s0 p1 H2] <+1> [CB2 s0 p0 H3] <+0> [prio:B2 s0 p0 H3] <+0> [B2 s4 p1 H3] <-3> [F2 s0 p0 H0]
FACE 3 [CB1 s0 p0 H7] <-1> [CB1 s0 p1 H6] <-2> [box4 H4] <-2> [box2 H2] <-2> [CD1 s0 p0 H0] <+0> [C1 s0 p0 H0]
FACE 4 [CB1 s0 p1 H10] <+1> [CB1 s0 p0 H11] <+0> [prio:B1 s1 p0 H11] <-3> [C2 s3 p1 H8] <-1> [CB2 s0 p0 H7] <-1> [CB2 s0 p1 H6] <-2> [box4 H4]
FACE 5 [ED1 s0 p0 H6] <-1> [D1 s7 p1 H5] <-1> [D1 s7 p0 H4] <-1> [prio:E2 s1 p0 H3] <-1> [prio:E2 s1 p1 H2] <+0> [ED2 s0 p0 H2] <-2> [box0 H0]
FACE 6 [ED2 s0 p0 H6] <-1> [D2 s4 p1 H5] <-1> [D2 s4 p0 H4] <-1> [B2 s0 p1 H3] <-1> [B2 s0 p0 H2] <+0> [AB2 s0 p0 H2] <-2> [box0 H0]
FACE 7 [AB1 s0 p0 H6] <-1> [A1 s0 p0 H5] <-1> [AF1 s0 p0 H4] <-2> [box2 H2] <-2> [box0 H0]
FACE 8 [AB2 s0 p0 H6] <-1> [A2 s0 p0 H5] <-1> [A2 s0 p1 H4] <-1> [B1 s0 p1 H3] <-1> [B1 s0 p0 H2] <+0> [AB1 s0 p0 H2] <-2> [box0 H0]
FACE 9 [CD2 s0 p0 H0] <-1> [CD2 s0 p1 H-1] <+0> [C2 s0 p0 H-1] <-3> [D1 s0 p0 H-4] <+0> [CD1 s0 p0 H-4] <-2> [box-6 H-6]
FACE 10 [CD2 s0 p1 H11] <+1> [CD2 s0 p0 H12] <-2> [box10 H10] <-2> [AF2 s0 p1 H8] <-1> [AF2 s0 p0 H7] <+0> [F2 s4 p1 H7] <-3> [D2 s0 p0 H4]
FACE 11 [AF1 s0 p0 H8] <+0> [F1 s6 p3 H8] <-1> [F1 s6 p2 H7] <-3> [A2 s2 p1 H4] <-1> [AF2 s0 p0 H3] <+1> [AF2 s0 p1 H4] <-2> [box2 H2]
FACE 12 [A1 s0 p0 H9] <-3> [B1 s0 p0 H6] <+1> [B1 s0 p1 H7] <-3> [A2 s1 p1 H4] <-1> [F1 s6 p2 H3] <+1> [F1 s6 p3 H4]
FACE 13 [B2 s0 p0 H6] <+1> [B2 s0 p1 H7] <-3> [D2 s3 p0 H4] <-1> [B1 s1 p1 H3] <-3> [A2 s0 p1 H0] <+1> [A2 s0 p0 H1]
FACE 14 [A2 s1 p1 H8] <-1> [B1 s1 p1 H7] <-3> [D2 s2 p0 H4] <-1> [F1 s5 p2 H3]
FACE 15 [A2 s2 p1 H8] <-1> [F1 s5 p2 H7] <-3> [D2 s1 p0 H4] <-1> [F2 s4 p1 H3]
FACE 16 [D1 s3 p0 H4] <-1> [F1 s4 p2 H3] <-3> [D2 s2 p0 H0] <-1> [B1 s2 p1 H-1]
FACE 17 [D2 s3 p0 H0] <-1> [B2 s1 p1 H-1] <-3> [D1 s4 p0 H-4] <-1> [B1 s2 p1 H-5]
FACE 18 [B1 s3 p1 H7] <-3> [F1 s3 p1 H4] <-1> [F1 s3 p2 H3] <-3> [D1 s3 p0 H0]
FACE 19 [B1 s3 p1 H11] <-3> [D1 s4 p0 H8] <-1> [B2 s2 p1 H7] <-3> [F1 s2 p1 H4]
FACE 20 [B1 s4 p1 H7] <-3> [F2 s2 p0 H4] <-1> [F2 s2 p1 H3] <-3> [D1 s2 p0 H0] <-1> [F1 s3 p2 H-1] <+1> [F1 s3 p1 H0]
FACE 21 [B1 s4 p1 H11] <-3> [F1 s2 p1 H8] <-1> [B2 s3 p1 H7] <-3> [F2 s1 p0 H4]
FACE 22 [B1 s5 p1 H7] <+0> [B1 s0 p0 H7] <-3> [C2 s1 p1 H4] <-1> [C2 s1 p0 H3] <-3> [D1 s1 p0 H0] <-1> [F2 s2 p1 H-1] <+1> [F2 s2 p0 H0]
FACE 23 [B2 s4 p1 H7] <+0> [B2 s0 p0 H7] <-3> [C2 s3 p1 H4] <-1> [prio:B1 s0 p0 H3] <+0> [B1 s5 p1 H3] <-3> [F2 s1 p0 H0]
FACE 24 [D2 s4 p0 H0] <+1> [D2 s4 p1 H1] <-3> [E2 s1 p1 H-2] <+1> [E2 s1 p0 H-1] <-3> [D1 s5 p0 H-4] <-1> [B2 s1 p1 H-5]
FACE 25 [B2 s2 p1 H11] <-3> [D1 s5 p0 H8] <-1> [E2 s0 p0 H7] <+0> [E2 s1 p0 H7] <-3> [F1 s1 p1 H4]
FACE 26 [B2 s3 p1 H11] <-3> [F1 s1 p1 H8] <-1> [E2 s0 p0 H7] <-3> [F2 s0 p0 H4]
FACE 27 [C1 s0 p0 H12] <-4> [D1 s0 p0 H8] <-1> [C2 s1 p0 H7] <+1> [C2 s1 p1 H8] <-1> [B1 s1 p0 H7]
FACE 28 [D2 s0 p0 H8] <-1> [F2 s3 p1 H7] <-3> [D1 s1 p0 H4] <-1> [C2 s0 p0 H3]
FACE 29 [C2 s2 p1 H0] <-1> [prio:B1 s1 p0 H-1] <-4> [B1 s1 p0 H-5]
FACE 30 [C2 s2 p1 H4] <-1> [B1 s0 p0 H3] <-4> [prio:B1 s0 p0 H-1]
FACE 31 [F2 s3 p1 H3] <-3> [D2 s1 p0 H0] <-1> [F1 s4 p2 H-1] <-3> [D1 s2 p0 H-4]
FACE 32 [prio:E2 s0 p0 H7] <-4> [E2 s0 p0 H3] <-3> [D1 s6 p0 H0]
FACE 33 [E2 s1 p0 H3] <-1> [E2 s1 p1 H2] <-4> [prio:E2 s1 p1 H-2] <+1> [prio:E2 s1 p0 H-1] <-3> [D1 s6 p0 H-4]
FACE 34 [E1 s0 p0 H6] <-3> [F1 s0 p0 H3] <+1> [F1 s0 p1 H4] <-1> [E2 s1 p0 H3] <+0> [prio:E2 s0 p0 H3] <-3> [D1 s7 p0 H0] <+1> [D1 s7 p1 H1]
FACE 35 [B2 s0 p0 H11] <-4> [prio:B2 s0 p0 H7]
FACE 36 [E1 s0 p0 H2] <-4> [prio:E1 s0 p0 H-2]
```

## Reflex vertices (18, all +1 = 45 degree bends turning away from the face)

Faces already convex: 0, 3, 5-9, 14-19, 21, 23, 25, 26, 28-32, 35, 36 (22 of 37). One reflex: 1, 2, 4, 10, 11, 20, 22, 27, 33. Two: 12, 13, 24, 34. Cusp faces (35, 36, 29, 30, 32: a lane and its row between split and merge) are digons/triangles with -4 corners; under D385 (ii) their cusp bend pairs are Shape's and they become convex trapezoids.

At a +1 reflex vertex (in h, out h+1) the interior angle is 225 degrees; a dummy edge leaves at d in {h, h-1, h-2, h-3} (d = h and d = h-3 give a straight angle on one side); Shape chooses d and the landing wall.
