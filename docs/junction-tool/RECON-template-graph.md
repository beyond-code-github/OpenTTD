# RECON-template-graph: what a graph-level ramp insert must supply (RC2)

Read-only recon, HEAD 43409e77b7. All line numbers are src/junction_nodes.cpp unless a file is named. Header = src/junction_nodes.h.

## 0. How the template is built (the one fact that frames everything)
BuildJunctionNodeTemplate (658-679) does `MakeJunctionCheckContext(flat, spec, FindJunctionPorts(flat), JunctionTMovements())` (:661). That traces the **drawing** into `ctx.paths[pi].pieces` (each piece: tile, trackdir, on_bridge, units) and keeps `ctx.layout` (the drawing). Then ReadPoints (137-347), AddPathDiffs (349-559), AddCrossings (561-630), AddBoxEdges (632-656), anchor (669-676). Every rule reads `ctx.paths` pieces and a few `ctx.layout` queries; nothing reads a "graph" object. The graph (path order, point kinds, link kinds) is **implicit**: points are recomputed from the piece sequence (:162-169), links from consecutive points (:202-256).
Piece units: `junction_route.cpp:70`: `on_bridge || IsDiagonalTrackdir (X/Y axis piece) ? 2 : 1` (a half piece = 1 unit, an axis piece = 2).

## 1. Fields of JunctionNodeTemplate and its structs

Legend: G = derivable from graph (path order, point kinds, link kinds, trackdirs, chosen lengths); D = needs drawing geometry (tile coordinates, sharing of tiles between paths, layout queries).

### JunctionNodePoint (h:25; built :190-192, classes :264-284)
| field | meaning | source |
|---|---|---|
| tile | flat tile of the piece | D. `pieces[i].tile` (:191). Absolute flat coordinate; only differences within a class/diagonal matter for the solve, but see §3 for what else reads absolute flat tiles |
| trackdir | trackdir of the piece | G (turn program gives it) |
| cls | X class | G given link kinds: union-find over rigid links and Y-free links (:267-271), numbered by first point |
| ycls | Y class | G: union over rigid links and X-free links (:267-271). Free diagonals join neither (:268) |

### JunctionNodeLink (h:32; :226-236, freeness :205-221)
| field | meaning | source |
|---|---|---|
| from,to | consecutive points of a path | G (deduped by (from,to), :226) |
| free | no on_bridge piece; same row X trackdirs, or same column Y trackdirs, or a diagonal | computed from piece trackdirs AND tiles (:209-210 compare `tile.y`/`tile.x` of every piece to the first); for a graph insert it is just the link kind: G |
| axis | axis of free link (:222) | G |
| diagonal | run >= 3 halves, whole run, no on_bridge, none under a Y-axis bridge (:214-220) | G for the run shape; the "under a Y-axis bridge" test reads `layout.bridges` and tile coordinates (:217-218): D, but only matters if a Y bridge is near |

### JunctionNodeDiagonal (h:47; :228-231)
| field | source |
|---|---|
| link | G |
| dx,dy | step of one map step = `TileIndexDiffCByDir(TrackdirToDirection(halves trackdir))` (:228): G (trackdir only) |
| halves | `b - a + 1`, flat half pieces from first to last point inclusive: a chosen length (G once chosen) |

### JunctionNodePath (h:68; :183-261)
| field | source |
|---|---|
| from,to | port roles: G |
| points | points in travel order: G |
| units | per point, flat unit at which its piece starts = running sum of `pieces[i].units` (:148-153,193): G once lengths are chosen (half = 1, axis = 2, bridge = 2) |
| dir | x sign of the path's X runs and free diagonals; from `diag.dx` or `tile.x` difference (:240-247,259): G for diagonals, D for X runs (tile differences, but a run's direction = its trackdir) |
| dir_y | same for y (:258-260). **Template error if a path has both signs** (:257-258) |

### JunctionNodeDiff (h:77), `diffs`
Built by NodeBuilder::Add (:78-86) from rules 1,2,3,4,5,6 and crossings. Constants in rhs come from `pos - flat[var]` offsets, i.e. tile coordinates (D by form, G by value: see §2).

### JunctionNodePiece (h:83; :319-323)
Pieces **not strictly inside a free link**, deduped by (tile, track): the point pieces and every piece of a rigid run. tile (D), track (G), on_bridge (G), cls, ycls = those of the governing (latest point at or before) point (:322).

### JunctionNodeJoin (h:91; :304-310)
Consecutive pieces on the ground, neither strictly inside a free link, with the same cls AND ycls: from_tile, from_td, to_tile, cls, ycls. Pairs of different class that are not the two ends of one free link are counted in `open_joins` and left to the oracle (:311-316). Source: piece sequence + tiles (D tiles; G which pairs).

### JunctionNodeBridge (h:99; :327-345): flat bridge with head classes. D (bridges come from `layout.bridges`; heads must be pieces of some path, :339).

### JunctionNodePrioRow (h:40; :456-463)
merge (G), side = minus the sign of (joiner's piece before M `.tile.y` - merge `.tile.y`) (:369, :462): **D** (tile rows), dir = path x dir (G), span = `spec.prio_tiles` (G). Only created for prio merges on main-line paths; unaffected by a diagonal insert except via the merge's class/point.

### JunctionNodeLateGap (h:54; :388-409)
path, p, q (G), g (rule gap, G from curve hands/spec), u = flat unit difference (:389: G once lengths chosen), diagonals between (G).

### JunctionNodeLateCrossing (h:63; :594-601): (bridge, diagonal) pairs where a diagonal's flat tile lies strictly between the heads on the bridge row (:585-587): **D** (tile coordinates of the diagonal's pieces).

### Scalars
| field | source |
|---|---|
| flat | per class, the flat x (X) / y (Y) of the class's first point (:275,:282): D (a tile coordinate, but only as a reference) |
| x_classes | G (:278) |
| anchor | X class of the last point of the EIn->SOut path (:675): G (a path/role query) |
| anchor_y | Y class of the first point of EIn->SOut (:676): G |
| open_joins, error | by-products |

## 2. Rules a new ramp (diag1 -> a=curve, free Y link, curve=d -> diag2) must produce

Points: p ..diag1.. a(last half) | b(first Y piece) ..Y.. c(last Y piece) | d(first half of diag2) ..diag2.. q. a-b and c-d are adjacent pieces => **rigid** links (:209-221: not free as they are not X/Y-same-line runs and not a diagonal run: 2 pieces). Classes that result from the rules: X: a,b,c,d one new X class (Y-free link joins X; rigid links join); p and q keep theirs (diagonals join nothing). Y: {a,b} one new Y class, {c,d} another (the Y-free link does not join Y classes). Diagonal links p->a and d->q free diagonals (need >= 3 halves each or they become rigid and join classes; the Y link needs >= 2 axis pieces b != c or it has no free link, b = c being one point).

| rule | where | derived from | graph-only possible? |
|---|---|---|---|
| curve hands | :437-448 | `DirDifference` of consecutive piece trackdirs (Left45/Right45) | Yes: trackdirs only |
| curve gaps T2 | :449-453 `JunctionCurveGaps(hands, tl, curve_speed, reference)` (junction_chain.h:39) then `add_gap(governing, start...)` | hands list of the whole path: the new two curves change the list for the whole path (every pair of curves, not just new ones), so **recompute the path's gaps**; U = flat unit difference between the curve points (:389) | Yes given lengths: units = halves*1 + axis tiles*2 (junction_route.cpp:70). add_gap classification (:392-417) is by link kinds: no X run and some Y run/diagonal -> static Y diff `AddDirected(d_y, ...) >= d_y*Δ + CeilHalf(g-u)` (:417); X run between -> late gap (:407-409); neither -> constant check (:412). Link kinds only. The two new curves have only the free Y link between them: a **static Y diff on b,c classes**, unit change = 2*d_y*Δy |
| unit gaps (rule 1 per link) | :421-434 | free link axis; free Y link: `AddDirected(d_y, At(from,Y), At(to,Y), 1)`; diagonal: m >= -((halves-2)/2) (:429-430) | Yes (kinds + halves) |
| late gaps | :407-409, solved :731-741 | any gap whose span contains an X run: includes curve pairs (previous curve, new curve), prio rule 3, slot rules 4-6 | Yes: link kinds; but they now also read `delta_y` and `steps` through the new Y classes |
| diagonal equality k | :726-727 (`k = (q.tile.x-p.tile.x) + dx*m`, m = dy*(Δ shift Y of q minus p), :704-709) | tiles of the two ends (flat displacement) and halves | **No, not from kinds alone**: needs flat tile of both ends consistent with `halves` (see §3) |
| prio coverage rule 3 | :455-470 | switch events (`SwitchEventsOf(layout, used, path)` :147: reads the **layout**), merge piece, curves/splits before it by unit | Needs events (which pieces are switches). Those pieces are old points; events of a path are G once the point list is given, but are computed from the drawing today. The new curves must be added to the "curves before the merge" list automatically (loop over curve_pieces, :464-466) |
| prio rows | :461-463 | `joiner_rows` from joiner piece tile.y (:369) | D, unchanged by the insert (merges are not new) |
| slot / waiting-room rules 4-6 | :472-557 | `is_slot` = `IsLegalSignalPiece(ctx.layout, piece) || IsSignalledPiece(ctx.layout, piece)` (:476): **reads the drawing** (neighbouring tracks of the piece's tile); `keep_slot` / `slot_is_fixed` read `link_of` | Not from kinds alone: a slot is a property of the piece in its tile surroundings. New pieces on the diagonal/Y: a half piece or axis piece with no neighbours is legal except where the signal-legality rule reads adjacent tracks, which for synthetic tiles must be answered |
| crossings | :561-630 | pieces of other paths whose **flat tile** is strictly between the bridge heads (:587); diagonals under an X bridge -> late crossing (:594-601) | D: tile coordinates of the new pieces decide whether they are "under" a bridge; a synthetic tile far from any bridge gives none. The existing diagonal's crossings (late_crossings referencing diag index) must be re-derived for diag1/diag2 (the removed diag index disappears) |
| occupancy rows/elements | h:136, :763-898 | `tpl.pieces`, `tpl.bridges`, links (raster interiors from p.tile, X-run interiors, Y-run interiors), prio rows; sorted per **current** row by x of Lo solution, ties and gaps by **flat** lo/hi and flat_row (:854, :878-890) | Elements come from G data (pieces, links), but each element carries `flat_row/flat_lo/flat_hi` = absolute flat tile coordinates (:784); gap rule `min(2, flat gap)` for elements on one flat row (:885). New elements need flat coordinates; if those collide with other flat elements the chain sees `now(a.hi) >= now(b.lo)` on a flat-unordered pair -> Occupancy failure (:879) |
| prio rows (§8) | :844-849 | merge point + side + span | unchanged |
| box edges (rule 6) | :632-656 | path end points at extreme flat x / y share a class | D (flat extremes) but unchanged: new points are interior |
| anchor | :669-676 | EIn->SOut path's first/last point classes | G; unaffected unless the repaired path is that path: still defined by path end points |

## 3. Data the solve needs per point/piece that only a drawing gives today

| datum | used at | what a graph insert can put there |
|---|---|---|
| `point.tile` / `piece.tile` (absolute flat tile) | classes' `flat[]` (:275,282); `y_of`, `NodeAddAtLeast` offsets (:695-701,:720); diagonal raster origin `p.tile` (:715); `x_p = p.tile.x + v - flat + offset` in all fits (:1065,1381,1402); late gaps/crossings | **Forced for consistency**: for each free link the flat coordinates must agree with its length: a diagonal link needs `q.tile == last tile of raster(p.tile, p.trackdir, halves)` (the solve never checks, but `diagonal_ok` :1165 builds q at `x_p + k, q.tile.y + shift` and the interior raster from p.tile: a mismatch makes the last `meets` non-adjacent); a Y link needs same column; rigid pieces adjacent. **Free choice**: the absolute position of a whole rigid class (only offsets inside classes matter to the solve), and the lengths (halves, Y run length). NOT a free choice: where the old p and q tiles sit — see geometry below |
| flat value as the snap's preferred value | `snap`: nearest to `tpl.flat[c]` (:1042-1058, `JunctionNodeSnap` :903); Y classes snapped first (:1335) | **Free choice per new class**: flat[] of the new X class and two Y classes = the reference the snap prefers. Any value in range works for the solver; a good choice is "the ramp is as short as it can be" (so the solve stretches the Y link only when facts/rules need it) |
| `flat_bits` (tracks of each flat tile) | built :1023-1037 from `tpl.pieces` and X/Y free-link tile fills (diagonal interiors excluded, :1028); `bits_at(tile)` used for all fits and joins (:1037,1065,1130,1143,1229,1381...) | For each new point/piece tile: bits = the track of the piece **plus any other path's track on the same flat tile** (a switch tile has 2+ tracks). A synthetic tile shared by nothing gets just its own track: correct as long as the tile really is unshared in the flat reference. If a synthetic flat tile coincides with another path's flat tile, `flat_bits` wrongly unions them (changes Switch vs Straight facts). `bits_at` throws (`.at`) for a tile that is neither a piece nor inside an X/Y free link: every point/piece tile used in a join must be in `pieces` |
| `pieces[]` tile+track (non-bridge) | `RenderJunctionNodeTracks` (:923-926), flat_bits, occupancy, fits | Must contain a, b, c, d (point pieces, class = governing point's): G once tiles are chosen. Interior Y and diagonal tiles are not stored (they come from the Y link range and the raster) |
| `joins` | :1062-1066, 1383 | a->b and c->d (same class and ycls each): add 2 joins; the Y link's end joins and the diag chains are implicit (:1393-1409, `diagonal_ok`, `column_ok`) |
| `flat_row/flat_lo/flat_hi` | occupancy (:784, 784-890) | tile coordinates again |
| path piece sequence | `w.start`, `governing`, `link_of`, `events`, `is_slot`, `curve_pieces` | All of AddPathDiffs is written against a piece vector with units (:380-559). A graph edit must hand it an edited piece vector, or the arrays must be rebuilt for that path |
| `ctx.layout` queries | `UsedTracks(ctx)` (:140), `SwitchEventsOf(layout,...)` (:147), `IsLegalSignalPiece/IsSignalledPiece(ctx.layout, piece)` (:476), `layout.bridges` (:217,:327) | Not available for a synthetic piece unless it is drawn |

### The geometry constraint on the flat state of the insert (matters for choosing a, b, c, d)
Half pieces travel along a diagonal line (TileIndexDiffCByDir of N/E/S/W: (+-1,+-1) per two halves, :228). A free diagonal is straight: p and q are on one line, and halves are 1 unit each. With diag1 (on a line through p), an axis ramp (Y run, (0,+-1) per piece) and diag2 (on a line through q): if p and q keep their flat tiles, then a (on p's line) and d (on q's line) are both on the old line L0 (p,q were joined by one diagonal), so d - a must equal n*(dx,dy). d - a = e1 + (L-1)*(0,t) + e2, where e1, e2 are the two curve steps (one axis step each: a half piece exits through an axis edge). Hence in the **flat state** the ramp straight must be short (a couple of tiles, whatever satisfies x: e1x+e2x = n*dx, y: e1y+e2y+(L-1)t = n*dy). That is fine for the template: the Y link is **free** (its length is a variable, rule 1 :433) and the solve stretches it by moving the Y classes {a,b} vs {c,d}; the diagonals absorb via m (:429-430, :726). The flat state is only the zero point of the variables. If instead you choose L larger than the geometry allows with p and q fixed, either p-side or q-side flat tiles (and every point rigidly or X-run/Y-run linked to them: whole classes, whole rows) must be translated in the flat, which moves them off the flat the other paths were drawn against (flat_bits, occupancy order, bridge crossings, box edges, prio rows all read flat tiles).
Direction constraint: the Y link must go in the same y sign as the diagonals on that path (`dir_y`, :258): a ramp that moves against the path's y-direction is a template error. The same for x with the X runs of the path.

## 4. Assessment

A graph-level insert is **possible** because every rule in AddPathDiffs/AddCrossings/AddBoxEdges and the whole solve consume only: `ctx.paths[].pieces` (tile, trackdir, on_bridge, units), points/links/classes/diagonals/pieces/joins computed from them, and four layout queries. Nothing about the *drawing* is needed beyond (1) absolute flat tile coordinates and (2) four layout queries (UsedTracks, SwitchEventsOf, IsLegalSignalPiece/IsSignalledPiece, layout.bridges). The smallest change that lets an insert produce an accepted template is therefore NOT "edit the template fields after the build" (that re-implements ReadPoints/AddPathDiffs for one link: new points, links, 2 diagonals, classes, flat[], pieces, joins, paths[].points/units for all later points, diffs for the whole path's curve gaps, rule-3/4-6 recomputation, late gaps, crossings) but one of:

**Option A (smallest, recommended): edit the traced path, not the layout.** Make BuildJunctionNodeTemplate take the paths as input (split `MakeJunctionCheckContext` from the builder: `NodeBuilder` only needs `ctx.paths` and `ctx.layout`, :54-56,:661). Do the graph edit as an edit of `ctx.paths[pi].pieces`: replace the diagonal's halves by diag1 halves + curve piece + axis pieces + curve piece + diag2 halves with synthetic tiles and `units` (1 per half, 2 per axis piece, :70) and keep every other piece unchanged. Then ReadPoints/AddPathDiffs/AddCrossings/AddBoxEdges run unchanged and derive classes, diagonals, pieces, joins, curve gaps, late gaps, crossings, occupancy inputs. Remaining drawing inputs to supply:
1. `ctx.layout` for `UsedTracks`, `SwitchEventsOf` (events at old switch pieces, unchanged) and `IsLegalSignalPiece/IsSignalledPiece` at **new** pieces (:476): either add the new tiles' tracks to a copy of the layout (the new tiles are in the flat reference; the layout is never rendered) or return false for them. Needs a decision: slot legality of the new pieces drives rules 4-6 (keep_slot, last_slot) on that path.
2. Flat tiles for a,b,c,d and the interior: see §3 (short ramp so p and q keep their tiles, tiles must not coincide with another path's flat tile, or `flat_bits`/occupancy break).
3. All other paths that share the diagonal link (links are deduped by (from,to), :226) must receive the same edit (an insert changes the link everywhere it appears).
4. The shared-diagonal `late_crossings` and bridge-under-diagonal rules reference the diagonal by index; they are recomputed by the rerun.

**Option B (template patch).** If the piece-vector edit is rejected: implement `InsertTemplateRamp(tpl, diag, ...)` that appends 4 points, 3 links (2 diagonals), 1 X class and 2 Y classes (flat entries), 4 pieces, 2 joins, rewrites the path's points/units (all later flat units shift by the length change, h:71) and recomputes every diff that involves a curve or unit span on that path. Needs the curve hands and `w.start/governing/link_of/events` of that path, which are not stored in the template (NodePathWork is builder-local, :38-43). This is a second implementation of AddPathDiffs; not smaller.

**Why not rebuild from a drawing:** nothing in the template requires an actual redrawn layout; but the flat state it implies (short ramp, rows and columns beside the old diagonal) must not collide with other flat tiles, since flat_bits (:1023-1037) unions tracks on a shared tile, and the occupancy chain treats overlap in the flat order as Occupancy failure (:879). If the tiles beside the old diagonal are used by another path in the flat, the insert cannot be done without moving that path's flat tiles: then the reference flat itself must change (a drawing), not just the graph.

**Facts that may surprise:**
- No code asserts diagonal flat consistency (`q.tile == raster end`); an inconsistent insert yields wrong joins/occupancy, not a Template error.
- `bits_at` (:1037) uses `.at()`: every join/piece tile must be in `pieces` or inside an X/Y free link.
- Y snap is blind to X (:52 of DESIGN-N3, §6); the ramp's X class {a,b,c,d} is a new variable restricted by column facts (`column_ok`) over the Y link.
- The new diagonals must have >= 3 halves each to stay free (:214); with fewer the link is rigid and joins classes, which pins the ramp.
