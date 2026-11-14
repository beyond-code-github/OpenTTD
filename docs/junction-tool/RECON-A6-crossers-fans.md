# Recon A6: crossers' pieces at item rows, and other rows' port fans (read-only; D271)

Head 7e1313e07a. jp = `src/junction/placement/junction_placement.cpp`, jg = `src/junction/junction_track_geometry.cpp` (line numbers of the unmodified files). Probe: `docs/junction-tool/wip/a6-crossers-fans-probe.patch` (hidden test `[probea6]`; a `ptype` field on `Occupant` and a `g_probe_fans` list in jp); full dump: `docs/junction-tool/wip/a6-crossers-fans-dump.txt`. Seeds T, LL_RR x L_R, L_R x LL_RR built as `RecordItemRowSeed` does, evaluated at `system.solved` (the recording's own solution). Anything not read or run is marked INFERRED.

## Summary
1. 48 (crosser, item row) pairs (T 6, LL_RR 30, L_R x LL_RR 12), 65 tiles (9, 41, 15); each pair has 1 or 2 occupants (2 whenever a bend covers it; 17 pairs). 32 Unders (4, 20, 8): every head-row cover is a candidate (0 outside): Run 18, next bend head 10 (Corner 6, SBend 4), previous bend tail 4. The 16 pairs on non-head rows are all covered by the same chain's pieces: 13 vertical-run pairs and 3 S-bend pairs.
2. No tile of any bend depends on L from the bend's first tile (`JunctionBendTileForms`: 0 tail tiles for all 16 heading x hand x kind combinations, L 1..9, so the "moves" tag at jp 1432 never fires). A crosser's column on item row y is `x[p] + c0 + cy (y - y_p)` with cy in {0, +1, -1} and no L term; L enters only the row range. 144 of 144 raster combinations and 48 of 48 seed pairs (65 of 65 tiles) are reproduced; 0 mismatches.
3. Exactly one piece (element or run) covers each of the 48 pairs: no overlap, no gap, no tie needed at the seeds. Candidate row conditions (below) partition by construction when a run lies between two elements.
4. The 24 skipped occupants are 4 fans' tiles on 2 other rows' item rows (e0 and e25 on row 1's items 3 and 4; e5 and e20 on row 2's items 6, 7, 8): 5 tiles + 5 + 7 + 7. W fans lie entirely west of every ranked occupant of the row, E fans entirely east, except that each shares a tile with the adjacent own-row fan (5 shared tiles, 4 touching pairs, all between adjacent same-side fans).
5. O2: 4 adjacent same-side fan pairs share rows (LL_RR only: W (e0,e1), (e19,e20); E (e5,e6), (e24,e25)); T and L_R x LL_RR have none. In all 4 every shared row is one of the four end rows (interior rows 0), so 0 tile pairs are left unordered by the four-end rows. sigma is equal inside every pair; the 4 pairs order west to east as e0 < e1, e20 < e19, e6 < e5, e25 / e24 tied (one shared tile).
6. Port tile and run of a foreign fan stand on another row's item row when the port row equals that item row: 4 times in LL_RR (port rows 4 and 8 = items 4 and 6; each a Tile and a Run); columns: tile `(port pt, 0)`, run `(port pt, +1)..(fan first pt, -1)` on the W side and `(fan last pt, +1)..(port pt, -1)` on the E side; the port points have fixed columns (0 and 61).

## 1. Crosser tiles

How read: for each crosser occupant of `item_rows` (kind "crosser") the probe finds the chain element (system.chains, copy 0 of the crosser edge) whose tile at the solved point equals the occupant, and separately enumerates every tile of every crosser on every item row of a row it crosses (`COVER` lines): the two agree (COVERTOTAL: tiles not recorded 0 at all three seeds). `moves`-tagged occupants (jp 1564) are never present (see 2).

Crosser covers per hub (item rows i, y; j = raster index of the tile in the bend, (dx, dy) from the bend's first tile):

| hub | crosser edge | element (k in chain, bend id, token, hin, hand, L, source) | item rows and tiles |
|---|---|---|---|
| T | e4 | Corner k1 b2 tok0 hin 1 hand +1 L2 fixed_k | i3 (y3): j3 (1,2), j4 (2,2) |
| T | e4 | vertical run after that Corner, to SBend b3 (first y 7) | i4, i5 |
| T | e8 | Corner k2 b12 tok3 hin 0 hand +1 L2 fixed_k | i3: j2 (1,-1), j3 (2,-1); i4: j0 (0,0), j1 (1,0) |
| T | e8 | vertical run after Heads k1 (y 6) to that Corner (first y 4) | i5 |
| LL_RR x L_R | e7 | Corner b5 hin 1 hand +1 L2; run after it (rows 4 to 15) | i3: j3 (1,2), j4 (2,2); run on i4..i11 (8 rows) |
| LL_RR x L_R | e13 | Corner b14 tok2 hin 0 hand +1 L2; run after Tile k0 | i3: j2, j3; i4: j0, j1; run on i5 |
| LL_RR x L_R | e8 | SBend k2 b7 tok5 hin 2 hand -1 L10 free | i6..i11: tiles (dx,dy) (1,1)(2,1), (2,2)(3,2), (3,3)(4,3), (4,4)(5,4), (5,5)(6,5), (6,6)(7,6) |
| LL_RR x L_R | e3 | vertical run after Heads k1 | i6..i11 |
| LL_RR x L_R | e11 | Corner b10 tok2 hin 0 hand -1 L2; run after Tile k0 | i9: j2 (-1,-1), j3 (-2,-1); i10: j0, j1; run on i11 |
| LL_RR x L_R | e17 | vertical run after Corner b22 hin 3 hand -1 | i9, i10, i11 |
| L_R x LL_RR | e6 | SBend k1 b9 tok0 hin 2 hand +1 L6 free | i3..i5: (-1,1)(-2,1), (-2,2)(-3,2), (-3,3)(-4,3) |
| L_R x LL_RR | e11 | vertical run after SBend b13 (y 31) to b16 (first y 11) | i3..i5 |
| L_R x LL_RR | e7, e12 | vertical run after Tile k0 | i3..i5 each |

Facts:
- Crosser covers on item rows are Corner and SBend tiles (T 6 tiles of Corner, 3 of run; LL_RR Corner 10, SBend 12, run 19; L_R x LL_RR SBend 6, run 9) and vertical runs. No Heads tile and no Tile element tile is on an item row. All bend tiles read as "head" tiles (offset from the bend's first point independent of L); every recorded crosser column has c1 = 0 and bend = -1.
- Run (`VRun`): jp 1394 loop, `(el.last, 0)`, rows strictly between `vy[el.last]` and `vy[next.first]`. The element before it is a Corner, an SBend, a Heads or a Tile (k0); the occupant's point is the element before's last point. Next present element: bend or none (the run goes to the node).
- Bend sources at the seeds: crossing Corners are `fixed_k` (L = k = 2), crossing S-bends over item rows are `free` (L from the columns, 6 and 10); `rows` S-bends (L 1) of e8 / e13 / e11 (T, LL_RR) stand on rows above the crossed item rows.
- Per Under (Under token u of crosser x at head row y of the copy): Run 18 (T 1, LL_RR 11, L_R x LL_RR 6), next bend head 10 (T 2 Corner, LL_RR 4 Corner + 4 SBend, L_R x LL_RR 0), previous bend tail 4 (T 1 Corner, LL_RR 1 Corner, L_R x LL_RR 2 SBend). The bend is the nearest bend token after / before the Under by `JunctionSystemBend::tok` (0 mismatches). A run tile does not say Before or After (it is one run, split by the token).
- Crossers on item rows that are not a head row of any of their Unders: 16 pairs (T 2, LL_RR 10, L_R x LL_RR 4); every one is an Approach-kind item row (stack kind 4: items 5, 8, 11 of LL_RR; item 5 of T and of L_R x LL_RR). Covered by: vertical run 13 pairs, S-bend tile pairs 3 (e8 at i8 and i11 of LL_RR, e6 at i5 of L_R x LL_RR). They are covered by the same chain pieces as the head rows. INFERRED: for the static model their candidate set is the same four (nearest bend token before and after, run) taken against the crosser's Under tokens of that row's bridges, or equivalently the piece whose row condition (section 2) holds.
- Occupants per pair: 1 (31 pairs) or 2 (17 pairs: a bend row has two adjacent tiles at columns w, w + 1; a Corner's horizontal last row (hin 1) has two; never 3).

## 2. Affine forms

Notation. A bend has first point p (column x[p], first tile row y_p = `vy[p]`, for a Fan its port row or item row as `row_pieces` puts it, jp 1235), last point q (row of its last tile is `vy[q]`), L its size (S-bend 2L tiles, Corner 2L + 1), r = d (y - y_p) the rows travelled, d its row direction. Verified: closed forms below equal `JunctionBendTileForms` rasters for all 144 (kind, hin, hand, L 1..9) combinations, and the crosser's recorded columns at the seeds (48 of 48 pairs, 0 mismatches, `FORMCHECK`).

Vertical start (hin 0 north, 2 south): d = -1 for hin 0, +1 for hin 2; e = +1 (east) when (hin + hand) mod 4 = 1, else -1 (hin 0 hand +1, hin 2 hand -1 east; hin 0 hand -1, hin 2 hand +1 west).
- Rows `0 <= r <= L - 1` (S-bend: all its rows; Corner: all but the last): two tiles, columns `x[p] + e r` and `x[p] + e r + e`. West = `x[p] + r`, east = west + 1 for e = +1; west = `x[p] - r - 1`, east = `x[p] - r` for e = -1. As `c0 + cy (y - y_p)`: cy = e d (+1 or -1), c0 = 0 (or -1 for the west tile when e = -1).
- Corner only, row `r = L` (y = `vy[q]`): one tile, column `x[p] + e L` (c0 = 0, c1 = e): the last tile of the Corner.
- In the column form (jg 76-114): columns t = 1..T(L) with rows `lo + s (t-1)..hi + s (t-1)`, T(L) = t0 + t1 L, are the same tiles: SBend hin 2 hand -1: lo 0 hi 1 s +1 t0 -1 t1 1 (T = L - 1); Corner hin 0 hand +1: lo -1 hi 0 s -1 t0 0 t1 1 (T = L). Column t = e (x[tile] - x[p]), so the diagonal covers rows `r = t - 1` and `t`. The column form's end columns (t = 0 and t > T(L)) are the rows r = 0 and r = L of the form above. Use the row form, it has no L in the column.

Horizontal start (hin 1 east, 3 west): c = +1 for hin 1, -1 for hin 3; d = hand for hin 1, -hand for hin 3.
- `r = 0`: one tile, column `x[p]`.
- `1 <= r <= L - 1`: two tiles, `x[p] + c (r - 1)` and `x[p] + c r`.
- `r = L`: S-bend one tile `x[p] + c (L - 1)`; Corner two tiles `x[p] + c (L - 1)`, `x[p] + c L`.
- A W/E fan is this S-bend with hin 1 (in edge on a W port, out edge on an E port) or hin 3; see 4 and 5.

Vertical run between elements a and b (jp 1393-1394): column `x[a.last]`, c0 = c1 = cy = 0; rows strictly between `vy[a.last]` and `vy[b.first]` (b the next present element).

y conditions under which a cover is the one on row y (in terms of points; L enters only as the row count):
- Run: `vy[a.last] < y < vy[b.first]` in the travel direction. Row `y` between the last row of a and the first row of b, both exclusive.
- Bend, vertical start S-bend: `y_p <= y <= vy[q]` in the travel direction (vy[q] - y_p = d (L - 1)); Corner: `y_p <= y <= vy[q]` (vy[q] - y_p = d L), the last row `y = vy[q]` has the single tile. Horizontal start S-bend: `y_p <= y <= vy[q]` with vy[q] - y_p = d L, both end rows single; Corner: same range, row y_p single, the rest two.
- Heads or Tile element (not on an item row at the seeds): the one tile (or two, at the two head points) at `vy[el.first]` and `vy[el.last]`.

Seed check (`FORMCHECK`): T 6 pairs reproduced of 6 (9 of 9 tiles); LL_RR x L_R 30 of 30 (41 of 41); L_R x LL_RR 12 of 12 (15 of 15); by cover: Corner hin 1 hand +1, Corner hin 0 hand +1 and -1, SBend hin 2 hand -1 and +1, vertical run. 0 mismatches. Covers never exercised at the seeds (horizontal S-bend crossers, vertical Corners hin 2, hands beyond those above) are checked only against the rasters (144 of 144), not against a recording.

## 3. Exclusivity

At the seeds each of the 48 (crosser, item row) pairs has exactly 1 element or run covering it (T 6, LL_RR 30, L_R x LL_RR 12); 0 pairs with 2 or more, 0 pairs with none. Within a pair a bend's two tiles are one cover. Between elements the run is strictly inside, the bends inclusive of their own end rows, so the conditions partition exactly when there is a run between two elements. INFERRED (read from the chain data, not run on an item row): the one case that is not a partition is two adjacent bends of one chain with `a.last == b.first` (T e8: Corner b12 ends on y 2 where S-bend b13 starts, y 2); both conditions hold on that row and add_bend writes two related occupants of the same edge for it. It does not occur on an item row of a row the edge crosses at the seeds, so no tie rule is needed there; an INFERRED proposal if it did: the tie rule is "the later element in travel order covers its own first row, the earlier one stops one row short", and the two tiles are one tile of one edge (related, no row between them).

## 4. Fans on item rows

LL_RR x L_R (T and L_R x LL_RR: 0; no fan of another row stands on an item row). The 8 fans (`FAN` lines; side from `JunctionPortSlots`, port row `system.port_row`):

| fan edge | in/out | slot | side | port row | item row (item, row) | hand | L | sigma | foreign item rows it stands on |
|---|---|---|---|---|---|---|---|---|---|
| e0 | in | 0 | W | 4 | 1 (1, row 0) | -1 | 3 | +1 | y3 (item 3), y4 (item 4), both row 1 |
| e1 | in | 1 | W | 5 | 3 (3, row 1) | -1 | 2 | +1 | none (own row) |
| e19 | out | 2 | W | 7 | 9 (7, row 2) | +1 | 2 | -1 | none |
| e20 | out | 3 | W | 8 | 12 (10, row 3) | +1 | 4 | -1 | y8, y9, y10 (items 6, 7, 8), row 2 |
| e5 | in | 6 | E | 8 | 11 (9, row 3) | -1 | 3 | -1 | y8, y9, y10 (items 6, 7, 8), row 2 |
| e6 | in | 7 | E | 7 | 8 (6, row 2) | -1 | 1 | -1 | none |
| e24 | out | 8 | E | 5 | 4 (4, row 1) | +1 | 1 | +1 | none |
| e25 | out | 9 | E | 4 | 1 (1, row 0) | +1 | 3 | +1 | y3, y4 (items 3, 4), row 1 |

Occupants (= the 24 skipped, `SKIPFAN`): e0 item 3 two Bend tiles (17,3) (18,3); item 4 Tile (0,4), Run (1..16, 4), Bend (17,4). e25 item 3 Bend (58,3) (59,3); item 4 Bend (59,4), Run (60,4), Tile (61,4). e5 item 6 Tile (61,8), Run (50..60, 8), Bend (49,8); items 7, 8 two Bend tiles each ((49,9) (48,9); (48,10) (47,10)). e20 item 6 Tile (0,8), Run (1,8), Bend (2,8); items 7, 8 two each ((3,9) (2,9); (4,10) (3,10)). Per item: item 3 has 4, item 4 has 6, item 6 has 6, item 7 has 4, item 8 has 4.

Columns against the row's ranked occupants (`item_rows[i]`, 28, 24, 33, 26, 9 per item, own in/out fans included): every W tile of a foreign fan is entirely west of all ranked occupants except one tile that shares the column with the adjacent own-row W fan (e0 vs e1: (18,3), (17,4); e20 vs e19: (2,8), (3,9)); every E tile is entirely east of all except e25's (59,4) shared with e24's. So in the static order of a row: foreign W fans precede, foreign E fans follow, every occupant of the row.

Fan-fan pairs of unrelated edges (`FANPAIR`, all rows): share a tile 5 (e0/e1 at (18,3), (17,4); e24/e25 at (59,4); e19/e20 at (2,8), (3,9)); touch (adjacent columns, same row) 4 (e0/e1 rows 3, 4; e19/e20 rows 8, 9). Nothing else shares or touches (T, L_R x LL_RR: 0 pairs). 3 of the SBend/Corner shares in the earlier recon (e8/e11) are crossers, not fans.

## 5. O2

Adjacent same-side fan pairs that share rows (`SIDEPAIR`):
- W (e0, e1): rows 3..4 shared; ends {4, 1, 5, 3}; both shared rows are end rows. Tiles e0 {17,18} vs e1 {18} (y3), e0 {17} vs e1 {17,18} (y4): shared tile on both rows.
- W (e19, e20): rows 8..9; ends {7, 9, 8, 12}; both end rows. y8 e19 {2,3} vs e20 {2}; y9 e19 {3} vs e20 {2,3}.
- E (e6, e5): row 8 only (e6's item row = e5's port row); y8 e6 {46} vs e5 {49}: strictly ordered.
- E (e25, e24): row 4 only; both stand on (59,4).
Four-end separation: in all 4 pairs every shared row is one of the four ends (interior shared rows 0), so rows at the four ends alone separate every pair of tiles; tile pairs not ordered by them: 0 of 0 interior; at the end rows the pairs are ordered at gap 0 (e0 < e1, e20 < e19, e25 vs e24 equal columns, shared) or strictly ordered (e6 at 46, e5 at 49). INFERRED: for fans whose rows overlap beyond their ends the lines of two fans of equal slope are parallel (every fan has slope +-1 column per row; its form is section 2's horizontal S-bend), so one interior row orders all; two fans of opposite sigma on one side would cross and need the ends of both. Not met at the seeds.
Order and sigma: sigma is equal in every adjacent pair (e0/e1 +1, e19/e20 -1, e6/e5 -1, e25/e24 +1). West to east at the pairs: e0 then e1 (N port first, W side, sigma +1), e20 then e19 (S port first, W side, sigma -1), e6 then e5 (N port first, E side, sigma -1), e25 / e24 tied. In geometry terms a same-sigma pair is two parallel diagonals: the one lying further south is east when the diagonal leans NE or SW and west when it leans SE or NW (e0/e1 NE: south one east; e19/e20 leans NW to its port: south one west; e5 leans SW: south one east). The notation orders ports N to S on a side (W side slot order; E side reversed), and that gives which fan is further south, so sigma and the notation together fix the order. INFERRED from the four pairs; sigma-different adjacent fans on one side do not occur at the seeds.

## 6. Port stubs and runs

Yes, in LL_RR x L_R only (T, L_R x LL_RR: 0): a foreign fan's port tile and the run between it and the fan stand on another row's item row whenever the port row equals that item row: e0 (port row 4 = item 4, row 1): Tile at (0,4) and Run (1..16, 4); e25 (port row 4 = item 4): Run (60,4), Tile (61,4); e5 (port row 8 = item 6, row 2): Tile (61,8), Run (50..60, 8); e20 (port row 8 = item 6): Tile (0,8), Run (1,8). Columns as recorded (`Occupant` left/right): port tile `(port pt, 0)` with the port point's fixed column (0 on W, 61 on E); run: W side `(port pt, +1)..(fan first pt, -1)`, E side `(fan last pt, +1)..(port pt, -1)` (jp 1409 `add_run`, west point then east point). The fan next to it sits at `(fan first pt, dx)` per section 2 (hin 1 or 3 horizontal S-bend). All four are non-empty at the seeds and all lie entirely west (W) or entirely east (E) of the row's ranked occupants like their fans.
