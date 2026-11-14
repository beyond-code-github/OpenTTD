# RECON: how coop passes a track under a bridge by a diagonal (study S1b)

Follow-up to RECON-corpus-crossings.md (S1). Script and raw output: `psg/study1b/` (s1b.py, sum.py, sum2.py, pics.py, out.pkl) on OneDrive. Read-only: nothing built or run in the game. The tool side is read from the code, not run.

## Headline
S1's "61% diag-S" lumps three different shapes. Of the 1481 diagonal-under-span groups:
- 42% (620) are true S-bends (the crossed track leaves on the heading it arrived on, stepping sideways);
- 45% (670) are a **Corner's diagonal** (the crossed track turns 90 degrees under the bridge);
- 13% (191) are a **doubled pair** of diagonals sharing tiles under one longer bridge.

The tool has a family for the first, none for the other two. The S-bend rows themselves match coop's plain S-bend shape; they are not the strict part.

## Part A: the tool's rule (read from the code)
Sources: placement/junction_placement.cpp lines ~2071-2130 (the "bridge over S-bend" family), junction_track_geometry.cpp (`BendRasterOffsets`, `BendColumnForm`), placement/CLAUDE.md line 30, DESIGN-G3 G3.4a, D248, D256.

Reading of the geometry (not run; I worked the raster by hand from `JunctionBendMoves`):
- An S-bend of size L is 2L half pieces: a staircase of L columns, Lower/Upper pairs. Column 0 holds one tile, columns 1..L-1 each hold a vertical domino of two tiles, column L one tile.
- Single bridge over it: the deck stands in sideways column t (t = 1 + `over` choice) and runs along the band edge's heading axis over that column's domino, **parallel to the heading** (code: bridge column = crossed start + side*t; heads at rows lo(t)-1 and hi(t)+1, both exact `eq`). The brief said "X-axis row" for a N/S edge; the code gives a Y-axis deck for a N/S edge. If the brief is right and the deck must cross the heading, the verdicts in Part C change (see Caveat).

Conditions:
| | rule |
|---|---|
| columns t | 1 <= t <= T(L) = T0 + T1 L, with T0 = -1, T1 = 1 by my hand raster, so t in 1..L-1 (the test P3 pins the form; the code check is `t <= t0 + t1*max(m,2)`) |
| bridge row | the deck covers exactly the column's two tiles; heads one tile beyond them, both pinned (no slack) |
| span | fixed: 2 middle tiles (4 tiles with heads) |
| heads | on column t, rows lo(t)-1 and hi(t)+1, so beside the neighbouring columns' stair tiles, not on the crossed track |
| which S-bend | the one just before or just after the crossed edge's Under/Above token (Corners of the crossing skipped); binary choice `over_after` (D248) |
| crossers | placement refuses a single bridge over several band crossings ("not ruled yet", line 350): exactly one crossed edge per single bridge, not doubled |
| curve gaps (D217) | `JunctionCurveGaps`, in half-tile units: adjacent curves of opposite hand >= 2; adjacent same hand >= CL (derived, capped at 2tl-1); curves i, j apart >= (j-i)*CL capped. The S-bend's two curves are opposite hands, so 2L-1 >= 2, i.e. L >= 2. Runs before/after the S-bend are bounded only by these gaps to the neighbouring bends, no separate floor |
| S-bend just before/after the crossing | allowed either side; the bridge row is pinned to the chosen S-bend |
| smallest | S-bend L = 2 (4 halves), t = 1, bridge 2 middle tiles; footprint along the heading = 4 rows (rows -2..1 around the bend's rows 0,-1) |
| tiles along the edge | max(L, 4) rows for t in 1..L-1 (the deck sits inside the bend's rows plus one head row each side only at the ends); the solver picks L, so the minimum is 4 |

The footprint note: at t=1 the head at row +1 and at t=L-1 the head at row -L lie just outside the bend's rows 0..-(L-1).

## Part B: coop's shapes (1481 diagonal groups in 294 hubs; deck axis A)
Method: for each bridge's under-tiles, groups of linked diagonal-only tiles (S1's class), then the crossed track traced outward both ways (up to 40 tiles) to the first non-diagonal piece, bend, node or open end. Tiles holding two pieces (191) could not be traced as one track and form the "doubled" class.

| class | groups | share |
|---|---|---|
| S-bend (same heading both ends, even piece count) | 620 | 42% |
| Corner diagonal (heading changes 90 degrees, odd piece count) | 670 | 45% |
| doubled pair (tiles with two pieces) | 191 | 13% |

**Every group has exactly 2 tiles under the span in a line** (a domino), except the doubled class.

**Bridge length (middle tiles)**: S-bend 2: 91% (n>2: 9%); Corner 2: 90%; doubled 3: 43%, 4: 32%, 5: 14%.

**S-bend (620)**
- Deck parallel to the crossed heading: 93% (575); across it (a row domino): 7% (45).
- Sideways step = L (rows or columns moved) = half the diagonal pieces: quartiles 5 / 9 / 14, max 18+; L = 2: 3%, L <= 3: 13%, L >= 4: 87%.
- Position t = (pieces before domino + 1)/2: t = 1: 15%, 2: 19%, 3: 11%, 4: 10%, rest spread to 20; t at either end (1 or L-1): 32%. Pieces before >= 3 (diagonal starts before the near head's column): 85%; pieces after >= 3: 80%; both: 67%; the domino is the whole diagonal (L = 2): 2%.
- Straight run to the next bend (of those that end in a bend): before median 9, after median 7; run <= 1 tile 6% / 7%; <= 2 tiles 10% / 9%. Over all S-bends the run ends in a node 23%/21% (switch right after the diagonal) and in a bend 56%/59%.
- Twin/double: only 6% of crossing bridges carry two separate diagonal groups, 1% three (single bridge over adjacent tracks).

**Corner diagonal (670)**: diagonal of 5 to 49+ pieces (quartiles 13 / 19 / 43; 5 pieces: 51), one arm parallel to the deck and the other across it (53% / 47%). Domino position: min(pieces before, after) is 1 (20%), 3 (17%), 5 (11%), 0 (9%): the bridge is usually near an end of the diagonal. The arm after the diagonal is a bend within 6 tiles for most; 44% have straight runs of 6+ on both arms.

**Doubled pair (191)**: two parallel diagonal tracks sharing tiles (Lower+Upper, Left+Right), under one bridge of 3-5 middle tiles (82/61/26), the pair spanning the whole deck.

### The five commonest shapes (all groups, pictures drawn from corpus instances, `B` head, `b` span, `^ v < >` diagonal pieces, `- |` straight)
1. Corner diagonal under a length-2 bridge, 601 groups, 41%. psg136_19 (hub box not given; save psg136, bridge (47,348)-(47,351)):
```
.......|
.......|
......v^
....Bv^.
....v^..
...v^...
..v^B...
--^.....
```
2. S-bend, length-2 bridge parallel to the heading, domino inside a long diagonal (L >= 4, t interior), ~464 groups, 31%. psg167_33 (537,343):
```
-->.....
..<>....
..B<>B..
....<>..
.....<->
```
3. Doubled pair under a 3-5 tile bridge, 191 groups, 13% (tiles Lower/Upper pairs, e.g. n = 3: `Lower | Lower+Upper | Upper`, n = 4: `Lower | Lower+Upper | Lower+Upper | Upper`; same stair twice, one tile apart).
4. S-bend, length 2, L = 3 (45) or L = 2 (12): 4%. psg173_45 (735,496) and psg144_6 (382,265):
```
-->....        ..v--
..<>...        Bv^B.
..B<>B.        .^...
....<--
```
5. S-bend with a longer bridge (n = 3..14, 54 groups, 4%) or the deck across the heading (45, 3%); psg136_21 (9,381)-(9,386):
```
.B..
.b..
.bv^
.v^.
v^..
.B..
```
Shapes 1-3 cover 85%; with 4 and 5, 92%; the rest are Corners under a longer bridge (69, 5%) and a few stragglers. A square run is 0.5% (S1).

## Part C: against the tool's conditions
| coop shape | share of groups | tool |
|---|---|---|
| S-bend, deck along heading, n = 2 (shapes 2 and 4) | 35% | **accepted**: t in 1..L-1 covers every coop domino (before and after pieces odd and >= 1 in 100%), heads are one tile beyond the domino. Only the curve gaps and the single crossing can bind: coop has a run of <= 2 tiles to the next bend in 17-20% (binds only if CL needs more) and L >= 2 holds |
| S-bend with a longer bridge (5) | 4% | rejected: the span is exactly 2 middle tiles |
| S-bend with the deck across the heading | 3% | rejected as read: no family has a 2-tile deck across the heading (the E/W-heading branch of the code gives a 1-tile deck) |
| **Corner diagonal under a bridge** (1) | 45% | **rejected**: no family; D256 offers only Corner, Under, Corner (the square run, 0.5% in coop) |
| doubled pair under one bridge (3) | 13% | rejected: a single bridge over several band crossings is refused (`crossed.size() != 1`, line 350) |

So the "S-bend rows are stricter than coop needs" suspicion is only half right. For a plain S-bend with a length-2 bridge the conditions are the same shape coop builds, and they admit all of coop's t. What is missing is everything else coop does under a bridge: the Corner's diagonal (45%), the doubled pair (13%), the longer span (4-5%).

### Tiles along the edge
The tool's minimum crossing: 4 rows (L = 2, t = 1, deck 4 tiles with heads; S-bend 2 rows inside). Coop's typical S-bend crossing: diagonal L median 9 (IQR 5-14) rows, plus straight stubs (median 7-10 tiles to the next bend). So coop uses more tiles, not fewer; the tile count is not what stops the tool. For a Corner under a bridge coop uses a diagonal of median 19 pieces (about 9 rows and 9 columns).

### Candidate rule changes (propose, not decide)
1. **Bridge over a Corner's diagonal**: column domino of the Corner staircase, heads one beyond, exactly as for the S-bend (the Corner's column form already exists: `JunctionCornerColumnForm`). Admits 45% of coop groups (41% at length 2).
2. **One single bridge over a doubled pair** (two band edges stepping together, 2 crossers): admits 13%; needs the lift of the one-crosser refusal and a paired column form (a Lower+Upper tile pair per domino row).
3. **Span as a floor, not exact**: deck >= 2 middle tiles with the domino anywhere in it, heads further out. Admits 4% of S-bends and about 5% of Corners; low value alone.
4. **Prefer these forms over the square run in the objective**: S-bend and Corner-diagonal first, square run last (coop 0.5%); no coop share to admit, but it matches practice.
5. **Deck across the heading** over a row domino: admits 3%; cheap only if the Y-axis machinery is mirrored.
Rules 1 + 2 together move the admitted share from 35% to 93%.

## Caveats and undone
- Orientation: the brief says "X-axis row" for a N/S band edge; the code puts the deck along the heading (Y for N/S). Coop has the deck along the heading in 93% of S-bends, matching the code. If the brief's reading is the real one (deck across the heading), the tool would reject those 93% and the S-bend verdict flips. One in-game or test dump of an over-S-bend layout would settle it; I did not run one.
- T0 = -1 and T1 = 1 come from my hand raster, not from the P3 test output.
- Corner and S-bend classes use the tracing cap of 40 tiles; some diagonals and runs reach the cap (a few dozen show 40 in before/after; not counted).
- Doubled class was not traced (the two tracks share tiles); its step and straight runs are not measured. "Doubled crossed track" is therefore 13% by tile sharing plus 6% by two separate groups on a bridge.
- The curve-gap CL value used by the tool (derived from the train) was not looked up; gaps are given symbolically.
- Hub role (band edge versus branch) is not separated, as in S1.
