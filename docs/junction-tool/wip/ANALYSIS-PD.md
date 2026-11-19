# ANALYSIS-PD: what the engineer changed in T3, line by line

Frame: absolute game tiles (solver layout + (100, 8)); x grows east, y grows south. Headings: E = +x, S = +y, NE = (+x, -y), SE = (+x, +y); every line here runs west to east, then the F lines turn south. A "piece" is one track piece (an axis piece is one tile; a diagonal piece is one tile holding half a diagonal step: two pieces move a line one tile sideways). Probe: `probe-pd`, dump `<S>/pd-lines.txt`.

## 1. Solver tracks labelled by line

- Print on `probe-pd` in test "planar: dump a mapped component layout": env `PLANAR_LINEDUMP=<path>`; one line per route-program piece `x y heading kind line route <route> copy <n>` (kind G ground, H deck head, M deck middle), then one line per prio-row piece `x y heading G prio:<lane>`. Program tiles are the layout JSON tiles (no shift needed). Result: 2541 lines (33 programs: 6 prio rows; routes A1-3, B1-3 (4+4+2 programs, B1 and B2 per path/copy), C1-3, D1-3, E1, E2 (2 copies), E3 (2 copies), F1-3).
- `pd-T3.json` is byte-identical to `t3-1796-o1t3f1s0.json` (`cmp`).
- Cross-check: decoding each ground piece into its track bit (entry side from the previous piece, exit from the next) reproduces every route bit of the layout exactly. The layout bits not explained by a route piece are the prio-row bits; 40 of the 145 dumped prio-row tiles (among them (133..151,8), (141..150,10), (143..148,12), (33..40,0) layout) have no track in the layout JSON.

## 2. The engineer's tiles labelled

- Method: tile pieces (tile, bit) and bridges are edges between tile-side nodes. A piece the engineer kept (same tile and bit) keeps the solver's label. Each new piece takes its label from the simple paths, inside x113..149 / y9..22, from the three EF in-lane ends (113,19..21) to the E ends ((149,9), (148,11), (147,13)) and to the F ends (the window's south edge, (142|141|140, 23)). A piece on both the E and the F path of lane i is `EFi`, on the E path only `Ei`, on the F path only `Fi`. All pieces are labelled (0 left).
- E2 and E3 have two parallel paths each (the two bridge copies); new pieces of both are labelled E2 / E3 (no copy number).
- Four kept straight pieces change line: (137,14) F1 -> E1; (132,16) and (133,16) F3 -> E2; (137,15) F2 -> E2.
- Window x108..152, y6..26, track pieces by line: new E1 19, E2 26, E3 20, EF1 26, EF2 15, EF3 21, F1 17, F2 22, F3 10 (176 in all); removed E1 17, E2 22, E3 19, EF1 29, EF2 24, EF3 19, F1 23, F2 24, F3 24 (201). Everything of A, B, C, D, CB and the prio rows in the window is unchanged; he only edited EF, E and F.

## 3. The change, line by line

Lines moved: EF1-3, E1-3, F1-3. The changed tiles are x113..148, y9..22.

Per line, solver route program (clipped to the window) against the engineer's path, both from the same anchors (EF in-lane tile x=113; E ends (149,9) (148,11) (147,13); F ends at y=23). Length: an axis piece 1, a diagonal piece 0.7071, a bridge its tile count; tiles = tiles occupied including bridge span; the EF prefix counts in both of a lane's routes. The solver programs also show 5 tiles x108..112 before x=113, not counted.

#### E1   (solver 46 tiles, 40.14 long; engineer 46 tiles, 40.14 long; +0 tiles, -0.00)
  solver, route E1 copy 0 (route program; window part):
    E    5 pieces  (108,19) -> (112,19)
    NE  20 pieces  (113,19) -> (122,9)
    E   30 pieces  (123,9) -> (152,9)
  engineer, path 1:
    E   13 pieces  (113,19) -> (125,19)
    NE  10 pieces  (126,19) -> (130,14)
    E   13 pieces  (131,14) -> (143,14)
    NE  10 pieces  (144,14) -> (148,9)
#### F1   (solver 53 tiles, 41.58 long; engineer 43 tiles, 35.09 long; -10 tiles, -6.49)
  solver, route F1 copy 0 (route program; window part):
    E    5 pieces  (108,19) -> (112,19)
    NE  20 pieces  (113,19) -> (122,9)
    E    9 pieces  (123,9) -> (131,9)
    SE  10 pieces  (132,9) -> (136,14)
    E    1 pieces  (137,14) -> (137,14)
    SE   9 pieces  (138,14) -> (142,18)
    S    8 pieces  (142,19) -> (142,26)
  engineer, path 1:
    E   13 pieces  (113,19) -> (125,19)
    NE  10 pieces  (126,19) -> (130,14)
    E    3 pieces  (131,14) -> (133,14)
    SE  17 pieces  (134,14) -> (142,22)
#### E2   (solver 46 tiles, 39.56 long; engineer 44 tiles, 38.73 long; -2 tiles, -0.83)
  solver, route E2 copy 0 (route program; window part):
    E    5 pieces  (108,20) -> (112,20)
    NE  18 pieces  (113,20) -> (121,11)
    E   10 pieces  (122,11) -> (131,11)
    bridge        (132,11) -> (135,11), 4 tiles
    NE   2 pieces  (136,11) -> (136,10)
    E    3 pieces  (137,10) -> (139,10)
    SE   2 pieces  (140,10) -> (140,11)
    E   12 pieces  (141,11) -> (152,11)
  solver, route E2 copy 1 (route program; window part):
    E    5 pieces  (108,20) -> (112,20)
    NE  18 pieces  (113,20) -> (121,11)
    E    8 pieces  (122,11) -> (129,11)
    NE   2 pieces  (130,11) -> (130,10)
    bridge        (131,10) -> (134,10), 4 tiles
    E    5 pieces  (135,10) -> (139,10)
    SE   2 pieces  (140,10) -> (140,11)
    E   12 pieces  (141,11) -> (152,11)
  engineer, path 1:
    E   15 pieces  (113,20) -> (127,20)
    NE  10 pieces  (128,20) -> (132,15)
    bridge        (133,15) -> (136,15), 4 tiles
    E    7 pieces  (137,15) -> (143,15)
    NE   8 pieces  (144,15) -> (147,11)
  engineer, path 2:
    E   15 pieces  (113,20) -> (127,20)
    NE   8 pieces  (128,20) -> (131,16)
    E    2 pieces  (132,16) -> (133,16)
    bridge        (134,16) -> (137,16), 4 tiles
    NE   2 pieces  (138,16) -> (138,15)
    E    5 pieces  (139,15) -> (143,15)
    NE   8 pieces  (144,15) -> (147,11)
#### F2   (solver 49 tiles, 39.33 long; engineer 37 tiles, 32.02 long; -12 tiles, -7.31)
  solver, route F2 copy 0 (route program; window part):
    E    5 pieces  (108,20) -> (112,20)
    NE  18 pieces  (113,20) -> (121,11)
    E    6 pieces  (122,11) -> (127,11)
    SE   8 pieces  (128,11) -> (131,15)
    E    6 pieces  (132,15) -> (137,15)
    SE   7 pieces  (138,15) -> (141,18)
    S    8 pieces  (141,19) -> (141,26)
  engineer, path 1:
    E   17 pieces  (113,20) -> (129,20)
    NE   6 pieces  (130,20) -> (132,17)
    E    3 pieces  (133,17) -> (135,17)
    SE  11 pieces  (136,17) -> (141,22)
#### E3   (solver 44 tiles, 38.14 long; engineer 42 tiles, 37.31 long; -2 tiles, -0.83)
  solver, route E3 copy 0 (route program; window part):
    E    5 pieces  (108,21) -> (112,21)
    NE  16 pieces  (113,21) -> (120,13)
    E    7 pieces  (121,13) -> (127,13)
    bridge        (128,13) -> (137,13), 10 tiles
    NE   2 pieces  (138,13) -> (138,12)
    E    3 pieces  (139,12) -> (141,12)
    SE   2 pieces  (142,12) -> (142,13)
    E   10 pieces  (143,13) -> (152,13)
  solver, route E3 copy 1 (route program; window part):
    E    5 pieces  (108,21) -> (112,21)
    NE  16 pieces  (113,21) -> (120,13)
    E    5 pieces  (121,13) -> (125,13)
    NE   2 pieces  (126,13) -> (126,12)
    bridge        (127,12) -> (136,12), 10 tiles
    E    5 pieces  (137,12) -> (141,12)
    SE   2 pieces  (142,12) -> (142,13)
    E   10 pieces  (143,13) -> (152,13)
  engineer, path 1:
    E   19 pieces  (113,21) -> (131,21)
    NE   6 pieces  (132,21) -> (134,18)
    bridge        (135,18) -> (139,18), 5 tiles
    E    2 pieces  (140,18) -> (141,18)
    NE  10 pieces  (142,18) -> (146,13)
  engineer, path 2:
    E   19 pieces  (113,21) -> (131,21)
    NE   4 pieces  (132,21) -> (133,19)
    E    2 pieces  (134,19) -> (135,19)
    bridge        (136,19) -> (140,19), 5 tiles
    NE  12 pieces  (141,19) -> (146,13)
#### F3   (solver 45 tiles, 37.09 long; engineer 31 tiles, 28.95 long; -14 tiles, -8.14)
  solver, route F3 copy 0 (route program; window part):
    E    5 pieces  (108,21) -> (112,21)
    NE  16 pieces  (113,21) -> (120,13)
    E    3 pieces  (121,13) -> (123,13)
    SE   6 pieces  (124,13) -> (126,16)
    E   11 pieces  (127,16) -> (137,16)
    SE   5 pieces  (138,16) -> (140,18)
    S    8 pieces  (140,19) -> (140,26)
  engineer, path 1:
    E   19 pieces  (113,21) -> (131,21)
    NE   2 pieces  (132,21) -> (132,20)
    E    5 pieces  (133,20) -> (137,20)
    SE   5 pieces  (138,20) -> (140,22)
TOTAL six routes: solver 283 tiles 235.84, engineer 243 tiles 212.25, delta -40 tiles -23.60

### Crossings in the window (E over F in both; nothing else crosses in x108..152, y6..26 except the unchanged B over F/D at y23-26)

| crossing | solver (bridge, under tiles) | engineer (bridge, under tiles) |
| --- | --- | --- |
| E2 over F1 | copy 1: (131,10)-(134,10), F1 under (132,10)(133,10); copy 0: (132,11)-(135,11), F1 under (133,11)(134,11) | copy A: (133,15)-(136,15), F1 under (134,15)(135,15); copy B: (134,16)-(137,16), F1 under (135,16)(136,16) |
| E3 over F2 | copy 1: (127,12)-(136,12), F2 under (128,12)(129,12); copy 0: (128,13)-(137,13), F2 under (129,13)(130,13) | copy A: (135,18)-(139,18), F2 under (136,18)(137,18); copy B: (136,19)-(140,19), F2 under (137,19)(138,19) |
| E3 over F1 | same two bridges as above, F1 under (134,12)(135,12) and (135,13)(136,13) | same two bridges as above, F1 under (137,18)(138,18) and (138,19)(139,19) |

- E1 crosses nothing (top lane) before and after. Over/under order is unchanged (E over F; E2 over F1; E3 over F2 and F1).
- Crossing moves (first under tile of the copy-1 / copy A bridge): E2 x F1 (132,10) -> (134,15): +2 x, +5 y. E3 x F2 (128,12) -> (136,18): +8 x, +6 y. E3 x F1 (134,12) -> (137,18): +3 x, +6 y.
- E3 bridge length 10 tiles -> 5 tiles (F2 and F1 were crossed 5-6 tiles apart on one long deck, now side by side: tiles (137,18) and (138,19) hold F1 and F2 together); E2 bridges 4 tiles -> 4 tiles.
- Where the crossing zone sits relative to the port rows: solver E bridges on rows 10-13 (the E port rows 9, 11, 13 plus the copy rows 10, 12); engineer E bridges on rows 15/16 (E2) and 18/19 (E3), E1 runs on row 14 with no bridge; each E line then climbs diagonally to its port row after its bridge (E1 (144,14)->(148,9), E2 (144,15)->(147,11), E3 (141..142,19|18)->(146,13)).

### Splits (where an EF lane parts into E and F)

| lane | solver split tile (row) | engineer split tile (row) |
| --- | --- | --- |
| EF1 | (132,9) row 9, after rising 10 rows | (133,14) row 14, after rising 5 rows |
| EF2 | (128,11) row 11, after rising 9 rows | (128,20) row 20, before any rise (E2 climbs from here, F2 runs straight 2 more tiles) |
| EF3 | (124,13) row 13, after rising 8 rows | (132,21) row 21, before any rise (E3 climbs 2-4 rows from here, F3 climbs 1 row) |

Split rows in the solver 9, 11, 13 (2 apart); engineer 14, 20, 21 (6 and 1 apart). Split order along x: solver EF3 (124), EF2 (128), EF1 (132); engineer EF2 (128), EF3 (132), EF1 (133).

### Turns after each bridge (first tile after the bridge end, and where the line's first turn is)

| line / copy | solver | engineer |
| --- | --- | --- |
| E2 copy 0 / B (the copy whose bridge is one tile later) | bridge ends (135,11); first tile after (136,11) is a diagonal piece: turns at once (up one row), (136,10), straight (137..139,10) 3 tiles, down one row (140,10)(140,11), straight to the port row | bridge ends (137,16); first tile after (138,16) is a diagonal: turns at once, up one row onto the other copy's row (138,15); then straight (139..143,15) 5 tiles; then the climb (144,15)->(147,11), 8 diagonal pieces, no step back |
| E2 copy 1 / A (earlier bridge) | bridge ends (134,10); first tile after (135,10) is straight; straight 5 tiles (135..139), first turn at (140,10) (6th tile): the shared step down | bridge ends (136,15); first tile after (137,15) straight; straight 7 tiles (137..143); first turn (144,15) (8th tile): the climb |
| E3 copy 0 / B | bridge ends (137,13); (138,13) diagonal: turns at once (up one row), (138,12), straight (139..141,12) 3 tiles, down one row (142,12)(142,13), straight to the port row | bridge ends (140,19); (141,19) diagonal: turns at once into the climb (141,19)->(146,13), 12 diagonal pieces |
| E3 copy 1 / A | bridge ends (136,12); straight 5 tiles (137..141,12), first turn (142,12) (6th tile): the step down | bridge ends (139,18); straight 2 tiles (140,141,18); first turn (142,18) (3rd tile): the climb (142,18)->(146,13), 10 diagonal pieces |
| D1 (154,14)-(160,14), D2 (154,17)-(161,17), B bridges | unchanged | unchanged |

Checked tile by tile: the engineer's statement "they turn immediately upon exit" is true of his B copies, but the solver already turns at once after the bridge on its copy-0 bridges (E2 (136,11), E3 (138,13)). What differs is what comes after the first turn. The solver's turn is a one-tile S-step (up one row, 3 straight tiles, down one row) that puts the line back on its lane row, and from there the line runs straight to the port (E2 12 tiles, E3 10 tiles). The engineer's turn after the bridge is the start of a climb that carries the line to the port row (E3 B) or is a one-row rise followed by a straight run and a climb (E2 B); his A copies run straight 7 and 2 tiles and then climb. In his version the lines never step back, so the bridge pair sits 5-6 rows south of the port rows, which is what allowed the crossings to move.

### Turns per line (run boundaries; heading changes)

Solver: EF/E1: E x5, NE (113,19)->(122,9), E to the east end (E1 turns twice in the window). E2: NE (113,20)->(121,11), E, bridge, S-step (up/down) after it, E; E3 likewise (NE (113,21)->(120,13)). F1: NE (113,19)->(122,9), E to (131,9), SE x10 to (136,14), E x1 (137,14), SE x9 to (142,18), S; F2: NE to (121,11), E to (127,11), SE x8 (128,11)->(131,15), E x6, SE x7 (138,15)->(141,18), S; F3: NE to (120,13), E x3, SE x6 (124,13)->(126,16), E x11, SE x5 (138,16)->(140,18), S. Each F line has two diagonal runs and a flat piece between them; the E lines have one climb before the split.

Engineer (full listing below): EF1: E to x125, NE x10 (126,19)->(130,14), E to the split (133,14); E1 then E to x143 and NE x10 (144,14)->(148,9) (two climbs in all); F1: SE x17 (134,14)->(142,22) (one diagonal run, no flat piece). EF2: E to x127; E2 NE x8-10 to row 15/16, bridge, E, NE x8 (144,15)->(147,11); F2: E to x129, NE x6 (130,20)->(132,17), E x3, SE x11 (136,17)->(141,22). EF3: E to x131; E3 NE x4-6 to row 18/19, bridge, climb NE x10-12 to (146,13); F3: E to x131, NE x2 (132,21)->(132,20), E x5, SE x5 (138,20)->(140,22). Net: EF lanes have no common climb; each E line has two climbs separated by the bridge zone; each F line has one diagonal run.

### Lengths (tiles occupied / length), solver -> engineer
E1 46/40.14 -> 46/40.14 (0). F1 53/41.58 -> 43/35.09 (-10 tiles). E2 46/39.56 -> 44/38.73 (-2). F2 49/39.33 -> 37/32.02 (-12). E3 44/38.14 -> 42/37.31 (-2). F3 45/37.09 -> 31/28.95 (-14). Six routes together 283 -> 243 tiles (-40), 235.84 -> 212.25 (-23.60). (Both copies of E2 and of E3 have the same length in each version.)



### Maps

Legend, window x108..152, y6..26 (two characters per tile, column header = x mod 100): letter = line family, digit = lane. E = E line, G = EF line, F = F line, D, B, C, J = CB, p = prio-row piece (`p.`), `==` = bridge span tile with no track under it, `*n` = n different lines on the tile (crossing tiles, parallel diagonal pairs, tiles holding both copies). Bridge heads carry their line. Bridges: solver E2 (131,10)-(134,10) and (132,11)-(135,11), E3 (127,12)-(136,12) and (128,13)-(137,13); engineer E2 (133,15)-(136,15) and (134,16)-(137,16), E3 (135,18)-(139,18) and (136,19)-(140,19). Unchanged in both: D1 (154,14)-(160,14), D2 (154,17)-(161,17), B bridges at y23-26.

Solver:

```
    8  9  10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49 50 51 52 
  6 .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  
  7 .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  
  8 .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  p. 
  9 .  .  .  .  .  .  .  .  .  .  .  .  .  .  G1 G1 G1 G1 G1 G1 G1 G1 G1 G1 *2 E1 E1 E1 E1 E1 E1 E1 E1 E1 E1 E1 E1 E1 E1 E1 E1 E1 E1 E1 *2 
 10 .  .  .  .  .  .  .  .  .  .  .  .  .  G1 G1 .  .  .  .  .  .  .  E2 E2 F1 F1 E2 E2 E2 E2 E2 E2 E2 .  .  .  .  .  .  .  .  .  .  p. p. 
 11 .  .  .  .  .  .  .  .  .  .  .  .  G1 *2 G2 G2 G2 G2 G2 G2 *2 E2 E2 E2 E2 F1 F1 E2 E2 .  .  .  E2 E2 E2 E2 E2 E2 E2 E2 E2 E2 E2 *2 E2 
 12 .  .  .  .  .  .  .  .  .  .  .  G1 *2 G2 .  .  .  .  E3 E3 F2 F2 == == == == F1 F1 E3 E3 E3 E3 E3 E3 E3 .  .  .  .  .  .  p. p. p. p. 
 13 .  .  .  .  .  .  .  .  .  .  G1 *2 *2 G3 G3 G3 *2 E3 E3 E3 E3 F2 F2 == == == == F1 F1 E3 E3 .  .  .  E3 E3 E3 E3 E3 E3 E3 *2 E3 *2 E3 
 14 .  .  .  .  .  .  .  .  .  G1 *2 *2 G3 .  .  .  F3 F3 .  .  .  .  F2 F2 .  .  .  .  F1 F1 F1 .  .  .  .  .  .  .  .  .  .  .  .  D1 D1 
 15 .  .  .  .  .  .  .  .  G1 *2 *2 G3 .  .  .  .  .  F3 F3 .  .  .  .  F2 F2 F2 F2 F2 F2 F2 *2 F1 .  .  .  .  .  .  .  .  .  .  D1 D1 .  
 16 .  .  .  .  .  .  .  G1 *2 *2 G3 .  .  .  .  .  .  .  F3 F3 F3 F3 F3 F3 F3 F3 F3 F3 F3 F3 *2 *2 F1 .  .  .  .  .  .  .  .  D1 D1 .  .  
 17 .  .  .  .  .  .  G1 *2 *2 G3 .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  F3 *2 *2 F1 .  .  .  .  .  .  D1 D1 .  D2 D2 
 18 .  .  .  .  .  G1 *2 *2 G3 .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  F3 *2 *2 F1 .  .  .  .  D1 D1 .  D2 D2 .  
 19 G1 G1 G1 G1 G1 *2 *2 G3 .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  F3 F2 F1 .  .  .  .  D1 .  D2 D2 .  .  
 20 G2 G2 G2 G2 G2 *2 G3 .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  F3 F2 F1 .  .  .  .  D1 D2 D2 .  D3 D3 
 21 G3 G3 G3 G3 G3 G3 .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  F3 F2 F1 .  .  .  .  D1 D2 .  D3 D3 .  
 22 .  .  .  .  p. p. p. p. p. p. p. p. p. p. p. p. p. p. p. .  .  .  .  .  .  .  .  .  .  .  .  .  F3 F2 F1 .  .  .  .  D1 D2 D3 D3 .  .  
 23 J3 J3 J3 J3 *3 B3 *2 B3 *2 B3 *2 B3 *2 B3 *2 B3 *2 B3 *2 B3 B3 B3 .  .  .  B3 B3 B3 B3 B3 B3 B3 F3 F2 F1 == B3 B3 B3 D1 D2 D3 == B3 B3 
 24 .  .  .  .  C3 C3 .  .  .  .  .  .  .  .  .  p. p. p. p. .  .  B3 B3 B3 B3 B3 .  .  .  B3 B3 == F3 F2 F1 B3 B3 B3 == D1 D2 D3 B3 B3 B3 
 25 .  .  .  .  .  C3 C3 .  .  .  .  .  .  .  p. *2 *2 B2 *2 B2 B2 B2 .  .  .  .  .  .  .  B2 B2 == F3 F2 F1 B2 B2 B2 == D1 D2 D3 B2 B2 B2 
 26 .  .  .  .  .  .  C3 C3 .  .  .  .  .  p. *2 *2 p. p. p. p. .  B2 B2 B2 B2 B2 B2 B2 B2 B2 B2 B2 F3 F2 F1 == B2 B2 B2 D1 D2 D3 == B2 B2 
```

Engineer:

```
    8  9  10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49 50 51 52 
  6 .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  
  7 .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  
  8 .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  p. 
  9 .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  E1 E1 E1 E1 *2 
 10 .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  E1 E1 .  .  p. p. 
 11 .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  E1 *2 E2 E2 E2 *2 E2 
 12 .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  E1 *2 E2 .  p. p. p. p. 
 13 .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  E1 *2 *2 E3 E3 *2 E3 *2 E3 
 14 .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  G1 G1 G1 G1 *2 E1 E1 E1 E1 E1 E1 E1 E1 E1 *2 *2 E3 .  .  .  .  D1 D1 
 15 .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  G1 G1 .  E2 E2 F1 F1 E2 E2 E2 E2 E2 E2 E2 E2 *2 E3 .  .  .  .  D1 D1 .  
 16 .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  G1 G1 .  E2 E2 E2 E2 F1 F1 E2 E2 .  .  .  .  E3 E3 .  .  .  .  D1 D1 .  .  
 17 .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  G1 G1 .  E2 E2 F2 F2 F2 F2 *2 F1 .  .  .  .  E3 E3 .  .  .  .  D1 D1 .  D2 D2 
 18 .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  G1 G1 .  E2 E2 F2 F2 .  E3 E3 F2 *2 F1 E3 E3 E3 E3 .  .  .  .  D1 D1 .  D2 D2 .  
 19 G1 G1 G1 G1 G1 G1 G1 G1 G1 G1 G1 G1 G1 G1 G1 G1 G1 G1 G1 .  E2 E2 F2 F2 .  E3 E3 E3 E3 F2 *2 F1 E3 E3 .  .  .  .  .  D1 .  D2 D2 .  .  
 20 G2 G2 G2 G2 G2 G2 G2 G2 G2 G2 G2 G2 G2 G2 G2 G2 G2 G2 G2 G2 *2 F2 F2 .  G3 *2 F3 F3 F3 F3 *2 *2 F1 .  .  .  .  .  .  D1 D2 D2 .  D3 D3 
 21 G3 G3 G3 G3 G3 G3 G3 G3 G3 G3 G3 G3 G3 G3 G3 G3 G3 G3 G3 G3 G3 G3 G3 G3 G3 .  .  .  .  .  F3 *2 *2 F1 .  .  .  .  .  D1 D2 .  D3 D3 .  
 22 .  .  .  .  p. p. p. p. p. p. p. p. p. p. p. p. p. p. p. .  .  .  .  .  .  .  .  .  .  .  .  F3 *2 *2 F1 .  .  .  .  D1 D2 D3 D3 .  .  
 23 J3 J3 J3 J3 *3 B3 *2 B3 *2 B3 *2 B3 *2 B3 *2 B3 *2 B3 *2 B3 B3 B3 .  .  .  B3 B3 B3 B3 B3 B3 B3 F3 F2 F1 == B3 B3 B3 D1 D2 D3 == B3 B3 
 24 .  .  .  .  C3 C3 .  .  .  .  .  .  .  .  .  p. p. p. p. .  .  B3 B3 B3 B3 B3 .  .  .  B3 B3 == F3 F2 F1 B3 B3 B3 == D1 D2 D3 B3 B3 B3 
 25 .  .  .  .  .  C3 C3 .  .  .  .  .  .  .  p. *2 *2 B2 *2 B2 B2 B2 .  .  .  .  .  .  .  B2 B2 == F3 F2 F1 B2 B2 B2 == D1 D2 D3 B2 B2 B2 
 26 .  .  .  .  .  .  C3 C3 .  .  .  .  .  p. *2 *2 p. p. p. p. .  B2 B2 B2 B2 B2 B2 B2 B2 B2 B2 B2 F3 F2 F1 == B2 B2 B2 D1 D2 D3 == B2 B2
```

## 4. Composer rules that keep the solver from producing his geometry

Facts only; "consistent" means the engineer's geometry satisfies the rule as far as read. Checker proof of any of these was not run (brief: no changes, no composer runs beyond the dump).
Files: `src/junction/planar/planar_components.cpp` (C), `planar_model.h` (M), `planar_model.cpp`, `planar_paths.cpp` (P), `planar_slots.cpp`.

### The E lines after their bridges (what holds a line straight)
1. Straight channel is a core on one heading. C:1402 `if (path.headings.size() == 1) { cores.push_back(c);` and `build_core` (C:1429 on): lead `Flex(lanes, h, offs, jog)` (C:1452), the decks, then `run.push_back(M.AddSeg(l, h, 0))` (C:1531), all on the channel's one heading `h`. The E channel (single east port) is such a core. There is no piece after the decks that can turn: the E lines cannot climb after the bridges (engineer: E1 NE x10, E2 NE x8, E3 NE x10-12 after the bridge zone).
2. The only sideways diagonal of a core is the optional jog before the decks: C:1171 `diag.push_back(M.AddSeg(l, Turn(h, jog), 2, std::nullopt, std::sqrt(2.0)))`, switched on by the variant's step bit (C:1450 `jog = cand_steps[k].toward`). Variant o1t3f1s0 has steps 0: no jog. It sits at the start of the core, one diagonal per lane, not between the decks and the port.
3. Lane rows through the core are slot offsets: `Flex` rows (C:1177-1178 `Eq(Dot(r, last[j].a) - Dot(r, last[0].a), 2 * offs[j])`), the run offset rows after the decks (C:1532-1533 `run offset`, `2 * post_offs[j]`), and the slot layer's `SlotBend` (C:898: S-bend by delta slots, "false (a failure) on a diagonal"). A lane that climbs a different number of rows than its neighbour after the crossing zone (engineer: E1 +5, E2 +5, E3 +6 rows; port rows 9/11/13) is not expressible; the composer only has one-slot S-bends.
4. The doubled copy's rejoin shape (C:1521-1525): `Double(lanes[j], h, sg, ..., 2)` (C:959-972: lane run, deck d0, step `Turn(h,sg)` exactly 2, copy step exactly 2, deck d1, `Tie` the copy's end to the lane's), then `M.AddSeg(lanes[j], h, 2)` (a run of at least 2 half-tiles) and `M.AddSeg(lanes[j], Turn(h, -sg), 2, 2)` (the step back, exactly one tile). So after the last deck: a one-tile step toward the copy side, a run, then a one-tile step back to the lane row. The engineer's copy joins (B rises one row onto A's row, E2 (138,16)->(138,15)) as the rule does, but has no step back; E3's B climbs immediately and for good.
5. Deck tails (C:1493-1494, last deck of a doubled pair): `M.Deck(d0, x0, ..., last_k ? 0 : nullopt)` copy 0 tail exactly 0 (so copy 0 turns on the first tile after its bridge: solver (136,11), (138,13); engineer B copies also do), copy 1 `... ? 4` tail exactly 4 half-tiles (two straight tiles after the bridge before the shared step, solver 5 tiles straight 135..139 includes them). Engineer's copy A runs 7 and 2 tiles straight after its bridge and is then on the climb; a tail run is `Deck(..., tail_min = 2)` only on single decks.
6. First deck heads (C:1493-1494 `first_k ? 4`, `first_k ? 0`): copy 0 near head exactly 4 half-tiles after its segment start, copy 1 exactly 0. Engineer: A copies run straight from the diagonal into the head (E2 (132,15)->(133,15), E3 (134,18)->(135,18)) = copy 1 pattern; consistent.
7. Equal bridges, copy 1 one tile upstream (C:1501-1502, `Eq(f0 - n0 - (f1 - n1), 0)` and `Eq(Dot(STEP[h], d0.a) + n0 - Dot(STEP[h], d1->a) - n1, 2)`): engineer's pairs are equal (4/4, 5/5) and A starts one tile before B in both: consistent.
8. Heads and deck ends on whole pieces: `Cross(d0, v, d1.has_value() ? 3 : 4)` (C:1480, C:1483 head 3 with a copy), `Deck` rows `a - n >= h`, `f - a >= h` (M:516-517) and `Parity(n, 0)`, `Parity(f, 0)` with `deck_parity = true` (M:520-521, M:264; set in the constructor, C:797). Engineer's bridges (4 and 5 tiles over a 2-tile and a 3-tile crossing) satisfy these as far as read: consistent.
9. Curve rows (planar_model.cpp:91 `PieceCurves`): consecutive opposite-hand curves at least 2, same hand G (`addrow(e - (curves[i].left_hand != curves[j].left_hand ? 2 : pr.g))`), curves k apart at least `min(2 * pr.tl - 1, (mand + 1) * pr.cl)`. The engineer's turns are 4+ half-tiles apart everywhere (his shortest: E3 A bridge end (139,18), 2 straight, curve at (142,18); the B S-step 1 tile): none found that fails.
10. Over crossings on axis pieces only (P:168 `is over a crossing on a diagonal piece`) and the under piece not parallel (P:169): the engineer's E bridges are on axis pieces and F crosses on a diagonal: consistent.

### The EF lanes and the fray (split rows)
11. Fray gap rows (C:1010 `M.Add(Eq(Dot(r, sp[k]) - Dot(r, sp[k - 1]), gaps[k - 1]))`; `gaps` = 2 x slot difference of the lanes in the last interval, C:1393 (`build_fray`); planar_slots.cpp:35 "The minimum spacing of consecutive lanes in the last interval of channel c: 2 when a prio Fray splits it", rows at planar_slots.cpp:124-125): the lanes' split points sit exactly `gaps` apart across travel. Solver: rows 9, 11, 13. Engineer: rows 14, 20, 21 (EF2 and EF3 split before rising). This is the rule that keeps EF2/EF3 from splitting on rows 20/21.
12. The in-lane's one rise: `Fray` (C:997-999) gives each in-lane `AddSeg(x, h, STUB)` (STUB = 4 half-tiles), `if (prio) M.AddSeg(x, Lt, 2, std::nullopt, 1.0)` (C:998; the one diagonal step left), then `AddSeg(x, h, 0, std::nullopt, FANW)`, then the split. The engineer's first rise of EF1 (126..130, 5 rows) fits this form (run x13 >= stub, one NE run, run, split); EF2 and EF3 have no rise before their split: the step-left is a required segment of length >= 2 in the prio fray, so an in-lane without it is not offered. Also the in-lanes' rise lengths 10, 9, 8 (solver) fan the lanes from spacing 1 to 2; engineer's lanes would be 5, 0, 0 rows at the split.
13. Fray crossings: `V_k crosses every nearer S_j; each crossed S_j decks over all farther V` (C:1017-1020, `PairDecks(d0, d1, far_v, h)`): E (straight S) over F (diverging V), E3 over both F1 and F2 in one deck pair: the engineer's relation is the same (E3 over F2 and F1, E2 over F1).
14. F bundle (C:1187-1260): per piece the lanes are offset-parallel (`bundle offset`, C:1253-1254 `Eq(Dot(r, piece_segs[c][lanes[j]][k].a) - Dot(r, piece_segs[c][lanes[0]][k].a), 2 * cur[j])`). Engineer's F lines enter their SE run at (134,14), (136,17), (138,20): across-travel offsets -x+y = -120, -119, -118, i.e. one lane spacing apart: consistent with a one-slot bundle; his F lines take one diagonal run (SE x17/11/5) with no flat piece, which the bundle's lead segments (`AddSeg(l, h, 0, ...)`, length >= 0) allow: none found that forbids it.
15. Channel headings and crossing pieces (P:226-246 "the short turn from start to end", crossing pieces on the first suitable piece): F turns E -> SE -> S, consistent.

### Not found
- No rule found that forbids F1-F3 going straight (single SE run); no rule found against E3's 5-tile bridges; no rule found against E1/E2/E3 bridges lying on rows 14-19 other than the lane-row rows (item 3) and core heading (item 1).
