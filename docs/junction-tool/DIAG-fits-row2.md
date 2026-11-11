# DIAG-fits-row2: four "fits" rejections in the [nodeoracle] gate (read-only diagnosis, base 305009e133)

Working coordinates throughout (layout offset + layout_offset). Terrain is flat Buildable except the Blocked rectangles. In every case the rejected tile is a Blocked tile (class 3) carrying track that exists only in the final layout, not in RenderJunctionNodeTracks(tpl, solve.values): it is laid by PlaceJunctionSignals, as part of the prio row of the merge at working (20,3). No rigid piece, X/Y run interior or diagonal raster tile is at any of the four tiles (checked: no template piece within 2 tiles on that row, no raw-render track at the tile).

ASCII legend: `-` X, `|` Y, `/` one diagonal track, `+` two or more tracks, `#` blocked empty, `B` blocked with track, `*` the failing tile, `.` empty. Columns are x = (failing x - 20) .. (failing x + 19); rows y=0..5. The RAW block is the track-only render (before PlaceJunctionSignals) at the same columns.

## Seed 100

- Blocked rects (x0..x1, y0..y1): x 30..32 y 5..5; x 20..21 y 0..2
- state 2 detours 1:  tile 20,2 cls 3 slope 0 bits 8 state.offset 14,-1 layout_offset 14,-1

Final layout (rows 0..5):
```
..............-----/##./-------------++-  y=0
....................##...............//.  y=1
..............--/...*B+-+-+-+-+-+-+././/  y=2
................/---+-+-+-+-+-+-+-+-+-.|  y=3
..................../-------------/....|  y=4
..............................###.//...|  y=5
```
Track-only render:
```
..............-----/##./-------------+--  RAW y=0
....................##...............//.  RAW y=1
..............--/...*#.............././/  RAW y=2
................/---+---------------+-.|  RAW y=3
..................../-------------/....|  RAW y=4
..............................###.//...|  RAW y=5
```

## Seed 113

- Blocked rects (x0..x1, y0..y1): x 19..19 y 12..12; x 23..25 y 1..2; x 36..37 y 11..12
- state 1 detours 0:  tile 23,2 cls 3 slope 0 bits 1 state.offset 14,0 layout_offset 14,-1

Final layout (rows 0..5):
```
...........-----------------------++-+-+  y=0
....................###...........//.../  y=1
...........--/.../-+*BB+-+-+-+-+././/.//  y=2
............./---+-+-+-+-+-+-+-+-+-.|//.  y=3
................./-------------/....||..  y=4
...............................//...|...  y=5
```
Track-only render:
```
...........-----------------------+-----  RAW y=0
....................###...........//.../  RAW y=1
...........--/......*##.........././/.//  RAW y=2
............./---+---------------+-.|//.  RAW y=3
................./-------------/....||..  RAW y=4
...............................//...|...  RAW y=5
```

## Seed 135

- Blocked rects (x0..x1, y0..y1): x 20..20 y 2..2; x 30..30 y 10..12
- state 1 detours 0:  tile 20,2 cls 3 slope 0 bits 8 state.offset 14,0 layout_offset 14,-1

Final layout (rows 0..5):
```
..............-----------------------++-  y=0
.....................................//.  y=1
..............--/...*-+-+-+-+-+-+-+././/  y=2
................/---+-+-+-+-+-+-+-+-+-.|  y=3
..................../-------------/....|  y=4
..................................//...|  y=5
```
Track-only render:
```
..............-----------------------+--  RAW y=0
.....................................//.  RAW y=1
..............--/...*..............././/  RAW y=2
................/---+---------------+-.|  RAW y=3
..................../-------------/....|  RAW y=4
..................................//...|  RAW y=5
```

## Seed 136

- Blocked rects (x0..x1, y0..y1): x 26..28 y 2..2
- state 1 detours 0:  tile 26,2 cls 3 slope 0 bits 9 state.offset 14,0 layout_offset 14,-1

Final layout (rows 0..5):
```
........-----------------------++-+-+-+-  y=0
...............................//.../---  y=1
........--/.../-+-+-*BB-+-+-+././/.//.-+  y=2
........../---+-+-+-+-+-+-+-+-+-.|//.../  y=3
............../-------------/....||..../  y=4
............................//...|.....|  y=5
```
Track-only render:
```
........-----------------------+--------  RAW y=0
...............................//.../---  RAW y=1
........--/.........*##......././/.//.-+  RAW y=2
........../---+---------------+-.|//.../  RAW y=3
............../-------------/....||..../  RAW y=4
............................//...|.....|  RAW y=5
```

## Per seed

| Seed | State | Failing tile | Bits | Element |
|---|---|---|---|---|
| 100 | 2 (1 detour) | 20,2 | 8 (Lower half) | signal-pass prio row, the end tile at x_M (blocked 20..21,0..2) |
| 113 | 1 | 23,2 | 1 (X) | signal-pass prio row interior (blocked 23..25,1..2) |
| 135 | 1 | 20,2 | 8 (Lower half) | signal-pass prio row, the end tile at x_M (blocked 20,2) |
| 136 | 1 | 26,2 | 9 (X + Lower) | signal-pass prio row interior (blocked 26..28,2) |

In all four the prio merge is the template's prio row with dir -1, side -1, span 14 (tpl.prio_rows): the merge point sits at (20,3) (point 34 in seed 100, point 26 in the others), so the reserved row is y=2, x 20..34 (x_M .. x_M + 14, p + 1 = 15 tiles). The first track of that row that is not free is the reported tile; the row's other tiles on blocked ground (21,2 in seed 100; 24,25 / 27,28) are rejected too, the loop just stops at the first.

## Why the solve's fact filter did not see it

SolveJunctionNodes step 3 (junction_nodes.cpp ~1180) checks only tpl.pieces (JunctionNodeTrackFits), tpl.joins, Y-free columns, the free X runs' end joins and free diagonal rasters. tpl.prio_rows is consumed in exactly one place: the occupancy chain (AddJunctionNodeOccupancy, ~line 845, DESIGN-N3 section 8), which only orders the reserved element against other elements on the row and never consults JunctionFacts. The prio row is not a template piece, so no piece filter covers it. Its tiles are drawn later by PlaceJunctionSignals (line 1294), after the solve, and the search's fits loop then finds them on Blocked tiles. The row's X at the merge is also not a free choice: its tiles are x_M .. x_M + 14 on row(M) - 1, so for a given X value of the merge's class and the Y snap of its row the whole 15-tile span is determined and could be filtered exactly like a rigid piece.

## Common cause

The prio row (DESIGN-N3 section 8) is a reserved occupancy element only: it is entered into the section 7 chain but never into the section 3 / step 3 "pieces fit the facts" filter (and, for heights, section 10 has no join for it either; heights currently stay clean only because the terrain here is flat). Step 3 should have covered it: for each prio_rows entry, in the merge's X class filter (and the merge's Y class snap, since the row is row(M) + side), require JunctionNodeTrackFits for every tile of the 15-tile span (X tracks, plus the half piece at x_M) at the candidate x and the snapped row. That is a rigid group of tiles hanging off the merge point, i.e. the same class of fact as section 10 (b) for a free run end, applied to the prio element. The other section 7 / section 8 elements (free X run interior, free Y run interior, diagonal raster) are already covered by step 3 or the Straight-failure path; the prio row is the only element type that reaches the layout without a fact check.

## Instrumentation

Scratch only (reverted): a getenv("DG2") dump at the fits-failure site in SearchJunctionNodes and a seed filter in the test; output in /tmp/claude-1000/dg2-out.txt.
