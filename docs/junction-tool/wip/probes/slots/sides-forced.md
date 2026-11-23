# sides-3: keep-apart separates a face's wall segments, cut at crossings (D431)
Change: a wall the face cuts at a crossing ends at the crossing point in the pair rows (and in the read-out, `PlanarKeepApartPair::Row::p_cross` / `q_cross`, `PlanarKeepApartEnd`). Same measurement as sides-2 (off-interface drawings included). Cell = forced share % / failures (forced count).

| fixture | drawings (off interface) | pair-instances | walk | walk_strict | facing | strict+facing | inward |
|---|---|---|---|---|---|---|---|
| X1 | 146 (144) | 3304 | 68.5% / 158 (2264) | 45.2% / 21 (1495) | 0.0% / 0 (0) | 45.2% / 21 (1495) | 24.5% / 268 (811) |
| X2 | 83 (82) | 2965 | 47.8% / 0 (1417) | 37.8% / 0 (1120) | 0.0% / 0 (0) | 37.8% / 0 (1120) | 25.9% / 453 (769) |
| M1 | 9 (0) | 397 | 47.4% / 3 (188) | 38.3% / 0 (152) | 0.0% / 0 (0) | 38.3% / 0 (152) | 21.4% / 69 (85) |
| K1 | 118 (80) | 1681 | 67.2% / 4 (1130) | 64.5% / 0 (1084) | 0.0% / 0 (0) | 64.5% / 0 (1084) | 18.3% / 215 (308) |
| L1 | 112 (111) | 2792 | 53.5% / 191 (1494) | 41.8% / 157 (1166) | 8.0% / 0 (224) | 41.8% / 157 (1166) | 30.4% / 368 (848) |
| Y1 | 8 (0) | 290 | 61.4% / 18 (178) | 42.8% / 2 (124) | 0.0% / 0 (0) | 42.8% / 2 (124) | 21.7% / 51 (63) |
| P1 | 1 (0) | 0 | 0.0% / 0 (0) | 0.0% / 0 (0) | 0.0% / 0 (0) | 0.0% / 0 (0) | 0.0% / 0 (0) |
| BR1 | 6 (0) | 122 | 54.1% / 3 (66) | 35.2% / 3 (43) | 0.0% / 0 (0) | 35.2% / 3 (43) | 27.9% / 11 (34) |
| TT1 | 2 (0) | 20 | 55.0% / 0 (11) | 30.0% / 0 (6) | 0.0% / 0 (0) | 30.0% / 0 (6) | 25.0% / 1 (5) |
| TS2 | 1 (0) | 22 | 86.4% / 0 (19) | 77.3% / 0 (17) | 31.8% / 0 (7) | 77.3% / 0 (17) | 40.9% / 0 (9) |
| 197 | 1 (0) | 2512 | 52.7% / 54 (1325) | 43.8% / 10 (1100) | 8.9% / 0 (223) | 45.1% / 10 (1134) | 41.5% / 410 (1043) |

walk_strict failures before -> after: X1 117 -> 21, X2 24 -> 0, L1 209 -> 157, Y1 5 -> 2, BR1 8 -> 3, TT1 1 -> 0, TS2 1 -> 0, 197 85 -> 10; M1 and K1 0 -> 0. Forced counts unchanged. Not yet explained: the remaining failures (first five per fixture in the commit report; not diagnosed here).

# sides-2: walk_strict failures checked, and measured on every legal drawing incl. off-interface (D431)

## Verdict on the walk_strict failures: not a mapping bug; the rule certifies the walked part of a wall, the rows use the whole piece
Hand check, X1 `sep 4 3 6`, polyline `A1:3*2,2*8,1*2,2*4,;AC1:2*4,;B1:0*10,...` (A1 = track 4, B1 = track 3). Heading 0 is north (STEP[0] = (0,-1), y down). Wall i = A1 piece 3 (walked heading 6), wall j = B1 piece 0 (walked heading 4); axes[0].s = STEP[(6+6)%8] = (0,1), south. (M1 and K1 have no crossing lines in their topology.)
- Points: A1 (4,5) (6,7) (14,7) (16,5) (20,5); B1 (11,10) (11,0). A1 piece 1 and B1 piece 0 cross at (11,7).
- Face 4's walk: t5 p0, t5 p1, box edge (north), A1 p3 (walked west, (20,5) to (16,5)), A1 p2 ((16,5) to (14,7)), A1 p1 (walked from (14,7) only to the crossing (11,7)), B1 p0 (walked from the crossing (11,7) to (11,10), heading 4). The last two walls are partial pieces: the face walk cuts each at the crossing.
- Candidates as the pair loop builds them (index = axis*2 + side; rows are the 4 whole-piece endpoint pairs): c0 s(0,1) from i, margin -6; c1 s(0,1) from j, margin -6; c2 s(1,0) from i, margin -10; c3 s(1,0) from j, margin +4 (A1 p3 lies east of B1, 5 to 9 columns). The legal drawing keeps c3.
- The rule: `monotone(i, j, (0,1))` holds (the walls after i have walked headings 5, 6, 4, dots 1, 0, 1 with (0,1)) and `guaranteed` = 1 >= spacing 1, so `walk_choice` returns 0 (the first branch wins). The other branch, `monotone(j, i, (1,0))`, is false (A1 p3 walked west has dot -1), so c3, the candidate that holds, is not certified.
- The read-out stores 0 = what the rule picks, in the numbering of `PlanarKeepApartHolds` (candidates are pushed axis*2 + side, side 0 = p on i, as `walk_choice`'s forced rows write them). No mapping error.
- Why c0 fails although the walk is monotone: the walk proves the walked part of B1 p0 (from y=7) lies south of A1 p3, but c0's rows use the whole piece, whose other endpoint (11,0) is at y=0, north of A1 (y=5), 7 columns behind the walk's start (against the normal). Wall j (and wall i in the other branch) is a partial piece when the face walk meets it at a crossing; the walk's guaranteed projection never counts the part of the piece outside the face.
- Pattern across fixtures: walk_strict failures occur exactly on the fixtures with crossings (X1, X2, L1, Y1, BR1, TT1, TS2) and are zero on M1 and K1 (no crossings). Rule-level issue, no fix in this step.

## sides-2 (off-interface drawings included)
Drawings now: every distinct polyline the domain accepts that the default-rule judge accepts, either with the interface rows (sides-1) or only without the port and box rows (off interface, from the read-out of that judge). 197 is as in sides-1 (V's points, 1 drawing). Cell = forced share % / failures (forced count).

| fixture | drawings (off interface) | pair-instances | walk | walk_strict | facing | strict+facing | inward |
|---|---|---|---|---|---|---|---|
| X1 | 146 (144) | 3304 | 68.5% / 254 (2264) | 45.2% / 117 (1495) | 0.0% / 0 (0) | 45.2% / 117 (1495) | 24.5% / 268 (811) |
| X2 | 83 (82) | 2965 | 47.8% / 24 (1417) | 37.8% / 24 (1120) | 0.0% / 0 (0) | 37.8% / 24 (1120) | 25.9% / 453 (769) |
| M1 | 9 (0) | 397 | 47.4% / 3 (188) | 38.3% / 0 (152) | 0.0% / 0 (0) | 38.3% / 0 (152) | 21.4% / 69 (85) |
| K1 | 118 (80) | 1681 | 67.2% / 4 (1130) | 64.5% / 0 (1084) | 0.0% / 0 (0) | 64.5% / 0 (1084) | 18.3% / 215 (308) |
| L1 | 112 (111) | 2792 | 53.5% / 243 (1494) | 41.8% / 209 (1166) | 8.0% / 0 (224) | 41.8% / 209 (1166) | 30.4% / 368 (848) |
| Y1 | 8 (0) | 290 | 61.4% / 29 (178) | 42.8% / 5 (124) | 0.0% / 0 (0) | 42.8% / 5 (124) | 21.7% / 51 (63) |
| P1 | 1 (0) | 0 | 0.0% / 0 (0) | 0.0% / 0 (0) | 0.0% / 0 (0) | 0.0% / 0 (0) | 0.0% / 0 (0) |
| BR1 | 6 (0) | 122 | 54.1% / 8 (66) | 35.2% / 8 (43) | 0.0% / 0 (0) | 35.2% / 8 (43) | 27.9% / 11 (34) |
| TT1 | 2 (0) | 20 | 55.0% / 1 (11) | 30.0% / 1 (6) | 0.0% / 0 (0) | 30.0% / 1 (6) | 25.0% / 1 (5) |
| TS2 | 1 (0) | 22 | 86.4% / 1 (19) | 77.3% / 1 (17) | 31.8% / 0 (7) | 77.3% / 1 (17) | 40.9% / 0 (9) |
| 197 | 1 | 2512 | 52.7% / 129 (1325) | 43.8% / 85 (1100) | 8.9% / 0 (223) | 45.1% / 85 (1134) | 41.5% / 412 (1043) | (unchanged from sides-1; one drawing)

Reading: walk_strict forces 30-65% on every fixture but fails on every crossing fixture (X1 117 of 1495, X2 24 of 1120, L1 209 of 1166, Y1 5, BR1 8, TT1 1, TS2 1) and on none of M1 and K1 (152 and 1084 forced, 0 failures); facing never fails (L1 224, TS2 7 forced); inward still fails often.

# sides-1 (before the check)
# sides-1: keep-apart sides forced by Shape's layout, measured on every legal drawing (D431)

Hidden test `[.][sidesmeasure]` (env `PLANAR_SIDES_REPORT`), 197 by `[planardomain197]` with env `PLANAR_SIDES_197`. Drawings: each distinct polyline of each fixture's configurations that the domain accepts and the default-rule judge accepts; 197 at V's own points (the domain calls them NOT legal and the judge refuses, so 197 failures do not prove too-strict). Share = forced pairs / pairs with rows, summed over drawings; failures = pair instances whose forced candidate does not hold on the drawing (the rule refusing a legal drawing). Candidate = axis*2+side.

Cell = forced share % / failures (forced count). For inward, forced = pairs with a single inward candidate; failures also count two-candidate pairs where neither inward candidate holds.

| fixture | drawings | pair-instances | walk | walk_strict | facing | strict+facing | inward |
|---|---|---|---|---|---|---|---|
| X1 | 2 | 38 | 71.1% / 3 (27) | 44.7% / 1 (17) | 0.0% / 0 (0) | 44.7% / 1 (17) | 23.7% / 2 (9) |
| X2 | 1 | 21 | 57.1% / 0 (12) | 42.9% / 0 (9) | 0.0% / 0 (0) | 42.9% / 0 (9) | 19.0% / 1 (4) |
| M1 | 9 | 397 | 47.4% / 3 (188) | 38.3% / 0 (152) | 0.0% / 0 (0) | 38.3% / 0 (152) | 21.4% / 69 (85) |
| K1 | 38 | 523 | 64.8% / 4 (339) | 59.8% / 0 (313) | 0.0% / 0 (0) | 59.8% / 0 (313) | 16.3% / 64 (85) |
| L1 | 1 | 9 | 77.8% / 1 (7) | 55.6% / 1 (5) | 22.2% / 0 (2) | 55.6% / 1 (5) | 33.3% / 0 (3) |
| Y1 | 8 | 290 | 61.4% / 29 (178) | 42.8% / 5 (124) | 0.0% / 0 (0) | 42.8% / 5 (124) | 21.7% / 51 (63) |
| P1 | 1 | 0 | 0.0% / 0 (0) | 0.0% / 0 (0) | 0.0% / 0 (0) | 0.0% / 0 (0) | 0.0% / 0 (0) |
| BR1 | 6 | 122 | 54.1% / 8 (66) | 35.2% / 8 (43) | 0.0% / 0 (0) | 35.2% / 8 (43) | 27.9% / 11 (34) |
| TT1 | 2 | 20 | 55.0% / 1 (11) | 30.0% / 1 (6) | 0.0% / 0 (0) | 30.0% / 1 (6) | 25.0% / 1 (5) |
| TS2 | 1 | 22 | 86.4% / 1 (19) | 77.3% / 1 (17) | 31.8% / 0 (7) | 77.3% / 1 (17) | 40.9% / 0 (9) |
| 197 | 1 | 2512 | 52.7% / 129 (1325) | 43.8% / 85 (1100) | 8.9% / 0 (223) | 45.1% / 85 (1134) | 41.5% / 412 (1043) |

## Reading
- facing never fails on any drawing (0 failures over every fixture and 197) but forces little: 0 on most fixtures, 2 pairs on L1, 7 on TS2, 8.9% on 197. Only anti-parallel facing walls are forced; facing walls at another angle choose between the two inward candidates on a reference, so the read-out forces none there (not computable without sides_from).
- walk and walk_strict refuse legal drawings (walk_strict: X1 1, L1 1, Y1 5, BR1 8, TT1 1, TS2 1; zero on X2, M1, K1). The union walk_strict+facing therefore fails wherever walk_strict does.
- inward (candidates restricted to the inward normals) fails often (M1 69, K1 64, Y1 51, BR1 11), so it is unsafe as a restriction.
- P1 has no pair with rows.

## First failures per rule (up to 5, per fixture)
```
X1: polylines 2 pair-instances 38 (inward two-way 29)
  walk: forced 27 (71.1%) failed 3
    sep 1 0 2 | A1:2*16,;AC1:2*4,;B1:0*10,;BD1:0*2,;C1:1*3,0*2,;D1:1*1,2*8,; | walls BD1/A1 pieces 0/0 | candidate 0 | margin -10
    sep 1 0 2 | A1:3*2,2*8,1*2,2*4,;AC1:2*4,;B1:0*10,;BD1:0*2,;C1:1*3,0*2,;D1:1*1,2*8,; | walls BD1/A1 pieces 0/1 | candidate 0 | margin -4
    sep 4 3 6 | A1:3*2,2*8,1*2,2*4,;AC1:2*4,;B1:0*10,;BD1:0*2,;C1:1*3,0*2,;D1:1*1,2*8,; | walls A1/B1 pieces 3/0 | candidate 0 | margin -6
  walk_strict: forced 17 (44.7%) failed 1
    sep 4 3 6 | A1:3*2,2*8,1*2,2*4,;AC1:2*4,;B1:0*10,;BD1:0*2,;C1:1*3,0*2,;D1:1*1,2*8,; | walls A1/B1 pieces 3/0 | candidate 0 | margin -6
  facing: forced 0 (0.0%) failed 0
  strict+facing: forced 17 (44.7%) failed 1
    sep 4 3 6 | A1:3*2,2*8,1*2,2*4,;AC1:2*4,;B1:0*10,;BD1:0*2,;C1:1*3,0*2,;D1:1*1,2*8,; | strict 0 facing -1
  inward(forced=1 cand): forced 9 (23.7%) failed 2
    sep 1 2 4 | A1:3*2,2*8,1*2,2*4,;AC1:2*4,;B1:0*10,;BD1:0*2,;C1:1*3,0*2,;D1:1*1,2*8,; | inward mask 1
    sep 4 0 4 | A1:3*2,2*8,1*2,2*4,;AC1:2*4,;B1:0*10,;BD1:0*2,;C1:1*3,0*2,;D1:1*1,2*8,; | inward mask 1
X2: polylines 1 pair-instances 21 (inward two-way 17)
  walk: forced 12 (57.1%) failed 0
  walk_strict: forced 9 (42.9%) failed 0
  facing: forced 0 (0.0%) failed 0
  strict+facing: forced 9 (42.9%) failed 0
  inward(forced=1 cand): forced 4 (19.0%) failed 1
    sep 1 0 2 | A1:2*16,;AC1:2*4,;B1:0*2,1*4,0*4,;BD1:0*2,;C1:1*3,0*2,;D1:1*1,2*8,; | inward mask 9 (two-way)
M1: polylines 9 pair-instances 397 (inward two-way 312)
  walk: forced 188 (47.4%) failed 3
    sep 4 3 5 | A1:2*12,;AB1:1*2,2*2,3*2,2*2,;AC1:2*4,;B1:0*4,1*3,;BD1:0*2,;C1:1*3,0*2,;D1:1*1,2*10,; | walls AB1/B1 pieces 0/0 | candidate 0 | margin -2
    sep 4 2 4 | A1:2*12,;AB1:1*2,2*4,3*2,;AC1:2*4,;B1:0*4,1*3,;BD1:0*2,;C1:1*3,0*2,;D1:1*1,2*10,; | walls AB1/B1 pieces 0/0 | candidate 0 | margin -2
    sep 3 2 4 | A1:2*12,;AB1:2*2,1*2,2*2,3*2,;AC1:2*4,;B1:0*4,1*3,;BD1:0*2,;C1:1*3,0*2,;D1:1*1,2*10,; | walls A1/AB1 pieces 0/1 | candidate 0 | margin -1
  walk_strict: forced 152 (38.3%) failed 0
  facing: forced 0 (0.0%) failed 0
  strict+facing: forced 152 (38.3%) failed 0
  inward(forced=1 cand): forced 85 (21.4%) failed 69
    sep 1 0 2 | A1:2*6,1*2,2*2,3*2,;AB1:2*8,;AC1:2*4,;B1:0*4,1*3,;BD1:0*2,;C1:1*3,0*2,;D1:1*1,2*10,; | inward mask 9 (two-way)
    sep 1 5 7 | A1:2*6,1*2,2*2,3*2,;AB1:2*8,;AC1:2*4,;B1:0*4,1*3,;BD1:0*2,;C1:1*3,0*2,;D1:1*1,2*10,; | inward mask 9 (two-way)
    sep 3 2 5 | A1:2*6,1*2,2*2,3*2,;AB1:2*8,;AC1:2*4,;B1:0*4,1*3,;BD1:0*2,;C1:1*3,0*2,;D1:1*1,2*10,; | inward mask 9 (two-way)
    sep 3 2 6 | A1:2*6,1*2,2*2,3*2,;AB1:2*8,;AC1:2*4,;B1:0*4,1*3,;BD1:0*2,;C1:1*3,0*2,;D1:1*1,2*10,; | inward mask 1
    sep 3 3 5 | A1:2*6,1*2,2*2,3*2,;AB1:2*8,;AC1:2*4,;B1:0*4,1*3,;BD1:0*2,;C1:1*3,0*2,;D1:1*1,2*10,; | inward mask 9 (two-way)
K1: polylines 38 pair-instances 523 (inward two-way 438)
  walk: forced 339 (64.8%) failed 4
    sep 2 2 4 | A1:1*3,0*6,;ABC1:2*4,;B1:2*2,1*2,2*2,3*2,2*4,;BC1:2*4,;C1:3*3,4*6,; | walls BC1/B1 pieces 0/1 | candidate 0 | margin -1
    sep 2 2 4 | A1:1*3,0*6,;ABC1:2*4,;B1:2*2,1*2,2*4,3*2,2*2,;BC1:2*4,;C1:3*3,4*6,; | walls BC1/B1 pieces 0/1 | candidate 0 | margin -1
    sep 2 2 4 | A1:1*3,0*6,;ABC1:2*4,;B1:2*4,1*2,2*2,3*2,2*2,;BC1:2*4,;C1:3*3,4*6,; | walls BC1/B1 pieces 0/1 | candidate 0 | margin -1
    sep 2 2 4 | A1:1*3,0*6,;ABC1:2*4,;B1:2*4,1*2,2*4,3*2,;BC1:2*4,;C1:3*3,4*6,; | walls BC1/B1 pieces 0/1 | candidate 0 | margin -1
  walk_strict: forced 313 (59.8%) failed 0
  facing: forced 0 (0.0%) failed 0
  strict+facing: forced 313 (59.8%) failed 0
  inward(forced=1 cand): forced 85 (16.3%) failed 64
    sep 3 1 4 | A1:1*3,0*6,;ABC1:2*4,;B1:2*12,;BC1:;C1:3*2,2*2,3*1,4*6,; | inward mask 9 (two-way)
    sep 3 2 4 | A1:1*3,0*6,;ABC1:2*4,;B1:2*12,;BC1:;C1:3*2,2*2,3*1,4*6,; | inward mask 9 (two-way)
    sep 1 5 7 | A1:1*3,0*6,;ABC1:2*4,;B1:2*12,;BC1:2*2,;C1:3*2,2*2,3*1,4*6,; | inward mask 1
    sep 3 1 4 | A1:1*3,0*6,;ABC1:2*4,;B1:2*12,;BC1:2*2,;C1:3*2,2*2,3*1,4*6,; | inward mask 9 (two-way)
    sep 3 2 4 | A1:1*3,0*6,;ABC1:2*4,;B1:2*12,;BC1:2*2,;C1:3*2,2*2,3*1,4*6,; | inward mask 9 (two-way)
L1: polylines 1 pair-instances 9 (inward two-way 6)
  walk: forced 7 (77.8%) failed 1
    sep 1 1 3 | A1:1*3,0*2,;A2:1*1,0*6,;AB1:;AB2:2*4,;B1:2*8,;B2:2*4,; | walls A2/B1 pieces 0/0 | candidate 0 | margin -4
  walk_strict: forced 5 (55.6%) failed 1
    sep 1 1 3 | A1:1*3,0*2,;A2:1*1,0*6,;AB1:;AB2:2*4,;B1:2*8,;B2:2*4,; | walls A2/B1 pieces 0/0 | candidate 0 | margin -4
  facing: forced 2 (22.2%) failed 0
  strict+facing: forced 5 (55.6%) failed 1
    sep 1 1 3 | A1:1*3,0*2,;A2:1*1,0*6,;AB1:;AB2:2*4,;B1:2*8,;B2:2*4,; | strict 0 facing -1
  inward(forced=1 cand): forced 3 (33.3%) failed 0
Y1: polylines 8 pair-instances 290 (inward two-way 227)
  walk: forced 178 (61.4%) failed 29
    sep 3 2 4 | A1:2*6,;A1#7:2*10,;A1#8:2*6,;A1#9:2*6,;AC1:;AD1:2*6,;B1:0*26,;BD1:;C1:1*9,0*6,;D1:1*11,; | walls A1/B1 pieces 0/0 | candidate 0 | margin -12
    sep 4 1 3 | A1:2*6,;A1#7:2*10,;A1#8:2*6,;A1#9:2*6,;AC1:;AD1:2*6,;B1:0*26,;BD1:;C1:1*9,0*6,;D1:1*11,; | walls A1#8/B1 pieces 0/0 | candidate 0 | margin -16
    sep 7 1 3 | A1:2*6,;A1#7:2*10,;A1#8:2*6,;A1#9:2*6,;AC1:;AD1:2*6,;B1:0*26,;BD1:;C1:1*9,0*6,;D1:1*11,; | walls A1#9/AD1 pieces 0/0 | candidate 0 | margin -4
    sep 1 3 5 | A1:2*6,;A1#7:2*10,;A1#8:2*6,;A1#9:2*6,;AC1:;AD1:2*6,;B1:7*2,0*2,1*2,0*20,;BD1:;C1:1*9,0*6,;D1:1*11,; | walls B1/A1#7 pieces 2/0 | candidate 0 | margin -2
    sep 3 2 4 | A1:2*6,;A1#7:2*10,;A1#8:2*6,;A1#9:2*6,;AC1:;AD1:2*6,;B1:7*2,0*2,1*2,0*20,;BD1:;C1:1*9,0*6,;D1:1*11,; | walls A1/B1 pieces 0/3 | candidate 0 | margin -6
  walk_strict: forced 124 (42.8%) failed 5
    sep 1 3 5 | A1:2*6,;A1#7:2*10,;A1#8:2*6,;A1#9:2*6,;AC1:;AD1:2*6,;B1:7*2,0*2,1*2,0*20,;BD1:;C1:1*9,0*6,;D1:1*11,; | walls B1/A1#7 pieces 2/0 | candidate 0 | margin -2
    sep 1 3 5 | A1:2*6,;A1#7:2*10,;A1#8:2*6,;A1#9:2*6,;AC1:;AD1:2*6,;B1:7*2,0*4,1*2,0*18,;BD1:;C1:1*9,0*6,;D1:1*11,; | walls B1/A1#7 pieces 2/0 | candidate 0 | margin -4
    sep 1 4 6 | A1:2*6,;A1#7:2*10,;A1#8:2*6,;A1#9:2*6,;AC1:;AD1:2*6,;B1:0*2,7*2,0*2,1*2,0*18,;BD1:;C1:1*9,0*6,;D1:1*11,; | walls B1/A1#7 pieces 3/0 | candidate 0 | margin -4
    sep 1 4 6 | A1:2*6,;A1#7:2*10,;A1#8:2*6,;A1#9:2*6,;AC1:;AD1:2*6,;B1:0*2,7*2,0*4,1*2,0*16,;BD1:;C1:1*9,0*6,;D1:1*11,; | walls B1/A1#7 pieces 3/0 | candidate 0 | margin -6
    sep 1 4 6 | A1:2*6,;A1#7:2*10,;A1#8:2*6,;A1#9:2*6,;AC1:;AD1:2*6,;B1:0*4,7*2,0*2,1*2,0*16,;BD1:;C1:1*9,0*6,;D1:1*11,; | walls B1/A1#7 pieces 3/0 | candidate 0 | margin -6
  facing: forced 0 (0.0%) failed 0
  strict+facing: forced 124 (42.8%) failed 5
    sep 1 3 5 | A1:2*6,;A1#7:2*10,;A1#8:2*6,;A1#9:2*6,;AC1:;AD1:2*6,;B1:7*2,0*2,1*2,0*20,;BD1:;C1:1*9,0*6,;D1:1*11,; | strict 0 facing -1
    sep 1 3 5 | A1:2*6,;A1#7:2*10,;A1#8:2*6,;A1#9:2*6,;AC1:;AD1:2*6,;B1:7*2,0*4,1*2,0*18,;BD1:;C1:1*9,0*6,;D1:1*11,; | strict 0 facing -1
    sep 1 4 6 | A1:2*6,;A1#7:2*10,;A1#8:2*6,;A1#9:2*6,;AC1:;AD1:2*6,;B1:0*2,7*2,0*2,1*2,0*18,;BD1:;C1:1*9,0*6,;D1:1*11,; | strict 0 facing -1
    sep 1 4 6 | A1:2*6,;A1#7:2*10,;A1#8:2*6,;A1#9:2*6,;AC1:;AD1:2*6,;B1:0*2,7*2,0*4,1*2,0*16,;BD1:;C1:1*9,0*6,;D1:1*11,; | strict 0 facing -1
    sep 1 4 6 | A1:2*6,;A1#7:2*10,;A1#8:2*6,;A1#9:2*6,;AC1:;AD1:2*6,;B1:0*4,7*2,0*2,1*2,0*16,;BD1:;C1:1*9,0*6,;D1:1*11,; | strict 0 facing -1
  inward(forced=1 cand): forced 63 (21.7%) failed 51
    sep 7 1 3 | A1:2*6,;A1#7:2*10,;A1#8:2*6,;A1#9:2*6,;AC1:;AD1:2*6,;B1:0*26,;BD1:;C1:1*9,0*6,;D1:1*11,; | inward mask 1
    sep 1 1 3 | A1:2*6,;A1#7:2*10,;A1#8:2*6,;A1#9:2*6,;AC1:;AD1:2*6,;B1:7*2,0*2,1*2,0*20,;BD1:;C1:1*9,0*6,;D1:1*11,; | inward mask 9 (two-way)
    sep 1 1 4 | A1:2*6,;A1#7:2*10,;A1#8:2*6,;A1#9:2*6,;AC1:;AD1:2*6,;B1:7*2,0*2,1*2,0*20,;BD1:;C1:1*9,0*6,;D1:1*11,; | inward mask 9 (two-way)
    sep 1 2 4 | A1:2*6,;A1#7:2*10,;A1#8:2*6,;A1#9:2*6,;AC1:;AD1:2*6,;B1:7*2,0*2,1*2,0*20,;BD1:;C1:1*9,0*6,;D1:1*11,; | inward mask 1
    sep 4 3 5 | A1:2*6,;A1#7:2*10,;A1#8:2*6,;A1#9:2*6,;AC1:;AD1:2*6,;B1:7*2,0*2,1*2,0*20,;BD1:;C1:1*9,0*6,;D1:1*11,; | inward mask 1
P1: polylines 1 pair-instances 0 (inward two-way 0)
  walk: forced 0 (0.0%) failed 0
  walk_strict: forced 0 (0.0%) failed 0
  facing: forced 0 (0.0%) failed 0
  strict+facing: forced 0 (0.0%) failed 0
  inward(forced=1 cand): forced 0 (0.0%) failed 0
BR1: polylines 6 pair-instances 122 (inward two-way 88)
  walk: forced 66 (54.1%) failed 8
    sep 1 3 5 | A1:2*16,;AC1:;B1:7*2,0*2,1*2,0*10,;BD1:;C1:1*3,0*6,;D1:1*3,2*6,; | walls B1/A1 pieces 2/0 | candidate 0 | margin -10
    sep 1 1 4 | A1:2*16,;AC1:;B1:0*2,7*2,0*4,1*2,0*6,;BD1:;C1:1*3,0*6,;D1:1*3,2*6,; | walls B1/A1 pieces 0/0 | candidate 0 | margin -10
    sep 5 1 3 | A1:2*16,;AC1:;B1:0*2,7*2,0*4,1*2,0*6,;BD1:;C1:1*3,0*6,;D1:1*3,2*6,; | walls B1/A1 pieces 3/0 | candidate 0 | margin -6
    sep 1 1 4 | A1:2*16,;AC1:;B1:0*4,7*2,0*2,1*2,0*6,;BD1:;C1:1*3,0*6,;D1:1*3,2*6,; | walls B1/A1 pieces 0/0 | candidate 0 | margin -10
    sep 5 1 3 | A1:2*16,;AC1:;B1:0*4,7*2,0*2,1*2,0*6,;BD1:;C1:1*3,0*6,;D1:1*3,2*6,; | walls B1/A1 pieces 3/0 | candidate 0 | margin -6
  walk_strict: forced 43 (35.2%) failed 8
    sep 1 3 5 | A1:2*16,;AC1:;B1:7*2,0*2,1*2,0*10,;BD1:;C1:1*3,0*6,;D1:1*3,2*6,; | walls B1/A1 pieces 2/0 | candidate 0 | margin -10
    sep 1 1 4 | A1:2*16,;AC1:;B1:0*2,7*2,0*4,1*2,0*6,;BD1:;C1:1*3,0*6,;D1:1*3,2*6,; | walls B1/A1 pieces 0/0 | candidate 0 | margin -10
    sep 5 1 3 | A1:2*16,;AC1:;B1:0*2,7*2,0*4,1*2,0*6,;BD1:;C1:1*3,0*6,;D1:1*3,2*6,; | walls B1/A1 pieces 3/0 | candidate 0 | margin -6
    sep 1 1 4 | A1:2*16,;AC1:;B1:0*4,7*2,0*2,1*2,0*6,;BD1:;C1:1*3,0*6,;D1:1*3,2*6,; | walls B1/A1 pieces 0/0 | candidate 0 | margin -10
    sep 5 1 3 | A1:2*16,;AC1:;B1:0*4,7*2,0*2,1*2,0*6,;BD1:;C1:1*3,0*6,;D1:1*3,2*6,; | walls B1/A1 pieces 3/0 | candidate 0 | margin -6
  facing: forced 0 (0.0%) failed 0
  strict+facing: forced 43 (35.2%) failed 8
    sep 1 3 5 | A1:2*16,;AC1:;B1:7*2,0*2,1*2,0*10,;BD1:;C1:1*3,0*6,;D1:1*3,2*6,; | strict 0 facing -1
    sep 1 1 4 | A1:2*16,;AC1:;B1:0*2,7*2,0*4,1*2,0*6,;BD1:;C1:1*3,0*6,;D1:1*3,2*6,; | strict 0 facing -1
    sep 5 1 3 | A1:2*16,;AC1:;B1:0*2,7*2,0*4,1*2,0*6,;BD1:;C1:1*3,0*6,;D1:1*3,2*6,; | strict 0 facing -1
    sep 1 1 4 | A1:2*16,;AC1:;B1:0*4,7*2,0*2,1*2,0*6,;BD1:;C1:1*3,0*6,;D1:1*3,2*6,; | strict 0 facing -1
    sep 5 1 3 | A1:2*16,;AC1:;B1:0*4,7*2,0*2,1*2,0*6,;BD1:;C1:1*3,0*6,;D1:1*3,2*6,; | strict 0 facing -1
  inward(forced=1 cand): forced 34 (27.9%) failed 11
    sep 1 1 3 | A1:2*16,;AC1:;B1:7*2,0*2,1*2,0*10,;BD1:;C1:1*3,0*6,;D1:1*3,2*6,; | inward mask 9 (two-way)
    sep 1 1 4 | A1:2*16,;AC1:;B1:7*2,0*2,1*2,0*10,;BD1:;C1:1*3,0*6,;D1:1*3,2*6,; | inward mask 9 (two-way)
    sep 1 2 4 | A1:2*16,;AC1:;B1:7*2,0*2,1*2,0*10,;BD1:;C1:1*3,0*6,;D1:1*3,2*6,; | inward mask 1
    sep 4 4 6 | A1:2*16,;AC1:;B1:7*2,0*2,1*2,0*10,;BD1:;C1:1*3,0*6,;D1:1*3,2*6,; | inward mask 1
    sep 3 3 5 | A1:2*16,;AC1:;B1:0*2,7*2,0*4,1*2,0*6,;BD1:;C1:1*3,0*6,;D1:1*3,2*6,; | inward mask 1
TT1: polylines 2 pair-instances 20 (inward two-way 15)
  walk: forced 11 (55.0%) failed 1
    sep 3 2 4 | A1:3*2,2*12,1*2,;AC1:;B1:0*16,;BD1:;C1:1*3,0*6,;D1:1*3,2*6,; | walls A1/B1 pieces 0/0 | candidate 0 | margin -2
  walk_strict: forced 6 (30.0%) failed 1
    sep 3 2 4 | A1:3*2,2*12,1*2,;AC1:;B1:0*16,;BD1:;C1:1*3,0*6,;D1:1*3,2*6,; | walls A1/B1 pieces 0/0 | candidate 0 | margin -2
  facing: forced 0 (0.0%) failed 0
  strict+facing: forced 6 (30.0%) failed 1
    sep 3 2 4 | A1:3*2,2*12,1*2,;AC1:;B1:0*16,;BD1:;C1:1*3,0*6,;D1:1*3,2*6,; | strict 0 facing -1
  inward(forced=1 cand): forced 5 (25.0%) failed 1
    sep 4 0 3 | A1:3*2,2*12,1*2,;AC1:;B1:0*16,;BD1:;C1:1*3,0*6,;D1:1*3,2*6,; | inward mask 1
TS2: polylines 1 pair-instances 22 (inward two-way 13)
  walk: forced 19 (86.4%) failed 1
    sep 1 1 3 | A1:1*3,0*6,;A2:1*3,0*14,;A3:1*3,0*22,;AB1:;AB2:2*2,;AB3:2*4,;B1:2*10,;B2:2*8,;B3:2*6,; | walls A2/B1 pieces 0/0 | candidate 0 | margin -2
  walk_strict: forced 17 (77.3%) failed 1
    sep 1 1 3 | A1:1*3,0*6,;A2:1*3,0*14,;A3:1*3,0*22,;AB1:;AB2:2*2,;AB3:2*4,;B1:2*10,;B2:2*8,;B3:2*6,; | walls A2/B1 pieces 0/0 | candidate 0 | margin -2
  facing: forced 7 (31.8%) failed 0
  strict+facing: forced 17 (77.3%) failed 1
    sep 1 1 3 | A1:1*3,0*6,;A2:1*3,0*14,;A3:1*3,0*22,;AB1:;AB2:2*2,;AB3:2*4,;B1:2*10,;B2:2*8,;B3:2*6,; | strict 0 facing -1
  inward(forced=1 cand): forced 9 (40.9%) failed 0

domain legal false judge accepts false
197: polylines 1 pair-instances 2512 (inward two-way 1469)
  walk: forced 1325 (52.7%) failed 129
    sep 2 13 16 | V's 197 | walls L1/L5 pieces 1/1 | candidate 0 | margin -12
    sep 2 17 19 | V's 197 | walls L5/L21 pieces 2/1 | candidate 0 | margin -2
    sep 4 1 3 | V's 197 | walls L22/L36 pieces 0/1 | candidate 0 | margin -2
    sep 4 4 6 | V's 197 | walls L36/L40 pieces 0/2 | candidate 0 | margin -1
    sep 7 1 5 | V's 197 | walls L91/L54 pieces 2/3 | candidate 0 | margin -1
  walk_strict: forced 1100 (43.8%) failed 85
    sep 2 13 16 | V's 197 | walls L1/L5 pieces 1/1 | candidate 0 | margin -12
    sep 7 1 5 | V's 197 | walls L91/L54 pieces 2/3 | candidate 0 | margin -1
    sep 8 4 8 | V's 197 | walls L93/L79 pieces 0/0 | candidate 0 | margin -3
    sep 8 5 8 | V's 197 | walls L11/L79 pieces 8/0 | candidate 0 | margin -3
    sep 9 4 10 | V's 197 | walls L85/L40 pieces 2/0 | candidate 0 | margin -2
  facing: forced 223 (8.9%) failed 0
  strict+facing: forced 1134 (45.1%) failed 85
    sep 2 13 16 | V's 197 | strict 0 facing -1
    sep 7 1 5 | V's 197 | strict 0 facing -1
    sep 8 4 8 | V's 197 | strict 0 facing -1
    sep 8 5 8 | V's 197 | strict 0 facing -1
    sep 9 4 10 | V's 197 | strict 0 facing -1
  inward(forced=1 cand): forced 1043 (41.5%) failed 412
    sep 0 0 2 | V's 197 | inward mask 1
    sep 1 3 5 | V's 197 | inward mask 9 (two-way)
    sep 1 3 6 | V's 197 | inward mask 1
    sep 1 4 6 | V's 197 | inward mask 1
    sep 2 1 19 | V's 197 | inward mask 1
```
