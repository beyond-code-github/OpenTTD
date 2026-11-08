# RECON-frame: the generator's fixed frame constants

Head a2028b8172. Read-only recon (brief: `briefs/FRAME-recon.md`). Line numbers are for that head. "Gen" is `src/junction_generator.cpp`.

Notation: `W` = 64 (frame width), `XH` = 30 (hub x, `xI`), `xO = XH + branch_gap + 1`, `yb = height - 1`, `C = tl * prio_tl` (prio coverage), `g = ml_gap`.

## 1. Table of every hard-coded or assumed frame constant

### 1a. Generator, `BuildJunctionCandidate` (Gen)

| file:line | constant / expression | meaning | depends on |
|---|---|---|---|
| Gen:195 | `xI = 30` | hub x in working coordinates | nothing (should be a function of C, sd, slack) |
| Gen:196 | `xO = xI + branch_gap + 1` | S-branch column | XH, branch_gap (fine as is) |
| Gen:205 | `base.min = tile(1, 0)` | router box left edge (N->W joins as far left as x = 2) | frame left edge 0; independent of XH |
| Gen:206 | `base.max = tile(xO + 16, yb)` | router box right edge. The 16 is the real right-hand search bound: it also clips the `61` windows below, because `RouteAndAdd` drops starts and goals outside the box (Gen:142-148). | XH, branch_gap; needs to grow with C (N->E goal starts at `x_s + C + sd + 2`) |
| Gen:225 | `StraightRoute(tile(0,0), X_SW, 63)` | E row: tiles x 0..63 | W (= 63 moves = W-1) |
| Gen:238 | `StraightRoute(tile(63, w_row), X_NE, 63)` | pass-1 W row: starts at x = 63, runs to x = 0 | W-1 |
| Gen:252 | `for x <= 63` (`w_tiles`) | crossable W row tiles | W-1 |
| Gen:258 | `for x <= 63` (`branch_fence`) | fence on rows 1..w_row-1 (ML gap rows) | W-1 |
| Gen:263 | `for x = 2; x <= xI - 2` | E->S start window (the split x_s) | left edge 2, XH |
| Gen:272 | `PlainRowPieces(..., xO + 2, 61)` | W->S start window upper bound | W-3; in effect clipped by box to xO+16 |
| Gen:273 | `lane_b_max_x` (derived, not a constant) | lane-B start window upper bound | lane B geometry |
| Gen:284 | `PlainRowPieces(l, 0, X_SW, max(xO, x_s + cover + sd + 2), 61)` | N->E goal window (D108): lower bound grows with C; upper bound 61 | W-3, clipped by box to xO+16 |
| Gen:294 | `route_movements(layout, true, {}, xI - 2, 61, true, w_row, 2)` | pass-1 N->W max x = XH-2; lane-B max x = 61; N->W min x = 2 | XH, W-3, left edge 2 |
| Gen:301 | `h_w < 0 \|\| h_e + 1 > 63` | bridge heads must fit: h_w >= 0, h_e + 1 <= 63 | W-1, left edge 0 |
| Gen:309 | `nw_max_for(merge) = min(xI - 2, merge - 1 - C - slack - (sd+1))` | N->W join upper bound | XH; formula already scales with C (good) |
| Gen:318 | `for k <= 63` (lane style 1, lane A search) | length of the straight run from the start | W-1 |
| Gen:319, 345, 361, 365, 378, 382 | `tile(63, w_row)` as lane program start | lane programs start at x = 63 | W-1 |
| Gen:343, 344 | `for p <= 63`, `for q <= 63` | lane-B search: p = straight run before RFL, q = run after RFL | W-1 (must reach from the start to h_e; start-to-head distance is ~63 - h_e) |
| Gen:360, 364, 381 | `for t <= 63`, `for s <= 63` | style-1 straight runs, lane tail s must reach x = 0 | W-1 |
| Gen:558 | `hub = {30 - shift_x, 0 - shift_y}` | hub offset in the returned layout | XH |
| Gen:784 (`FitJunctionLibrary`) | `ox = 30 - e.hub.x` | library entry offset into terrain | XH |

### 1b. Header and docs

| file:line | text | note |
|---|---|---|
| `junction_generator.h:53` | "working coordinate (30, 0)" | comment, update with XH |
| `junction_generator.h:76-77` | "x 0..63, rows from 0 ... anchor is the hub point (30, 0): cell (30 - origin.x, ...)" | comment, update |
| `junction_game.h:50` | `OrientJunctionVector({cx - 30, cy}, ...)` | comment |
| `junction_game.cpp:215` | "x - 30 ... grid covers x 0..63, y -2..40" | comment |

### 1c. Terrain snapshot, `junction_game.cpp`

| file:line | constant | meaning | depends on |
|---|---|---|---|
| game:225 | `origin = {-margin, -2 - margin}` | grid origin x = 0, y = -2 | frame left edge, row -2 (rows -2 and -1 exist because tests block row -1 and the router box reaches y = 0 with a 12 tile pad) |
| game:226 | `width = 64 + 2*margin` | frame width W | W |
| game:227 | `height = 43 + 2*margin` | rows -2..40, so y max = 40 | `yb` max = g + 19 (height <= g + 20), plus prio rows added by `PlaceJunctionSignals` below the branch edge; 40 gives headroom for g up to ~20 |
| game:238 | `gx + origin.x - 30` | the anchor maps to working x = 30 (D93) | XH. Changing XH changes which map tile is the hub for a given terrain; a pure function of the XH constant |
| game:277-289 (`JunctionNearOffsets`) | distances {2, 4} | near search; margin 4 is needed by the callers (`junction_gui.cpp:469, 558`, `console_cmds.cpp:355`) | independent of the frame size |

### 1d. Callers and other files (all clean)

- `junction_gui.cpp:469, 505, 558`, `console_cmds.cpp:355, 384`: take the margin (4 or 0) and use `result.hub` through `OrientJunctionOffset`. No literal 30/63/64/43. They work for any XH and W as long as `MakeJunctionTerrain` and the generator agree.
- `junction_terrain.h`: `width`, `height` and `origin` are data; `At()` returns nullptr outside the grid. No literal.
- `junction_router.cpp:95-98`: the grid is `request.min - 12 .. request.max + 12`, so the router adapts to any box. `max_expansions = 50000` (Gen:207) is the only fixed budget; a larger box may need more.
- `junction_signal_pass.cpp`, `junction_rules.cpp`, `junction_layout.cpp`, `junction_route.cpp`, `junction_station_rules.cpp`, `junction_train_model.cpp`, `junction_build.cpp`, `junction_ghost_draw.cpp`: no frame constants (the one `61` hit, `junction_rules.cpp:558`, is a speed). They work on the normalised layout, after the crop and shift.
- `tools/ottd/layouts/*.json` and the rig: no dependence found on XH (the `[9,2,30,14]` in `circuit-LR-TL4-prio2.json:351` should be confirmed as not the hub; it is a "junction" record, not read by the generator). Not checked beyond a grep.

## 2. The minimal parameter set

Define one struct, computed once at the top of `BuildJunctionCandidate` and by `MakeJunctionTerrain` (the terrain needs the same numbers, so they must come from one function `JunctionFrame MakeJunctionFrame(const JunctionSpec &, int max_height)`).

Let `dC = tl * (prio_tl - 1)`, the extra coverage beyond a prio of 1 (0 at prio 1, so the current numbers fall out).

| parameter | replaces | formula | value at prio 1, tl 4 |
|---|---|---|---|
| `XH` (hub x) | 30 at Gen:195, 558, 784, game:238, tests | `30 + dC`. Left of the hub live the E->S split window (x 2..XH-2), the N->W join (`merge - 1 - C - slack - sd - 1`) and the lane tails. The only term in them that grows with C is the N->W join distance from the merge, so the left side grows by exactly dC. A safer form: `XH = 2 + (C + slack_max + sd + 1) + L0`, with `L0` chosen so the result is 30 at the reference spec; the simplest correct choice is the `30 + dC` form. | 30 |
| `W` (frame width, tiles x 0..W-1) | 63, 64 at Gen:225, 238, 252, 258, 301, 318-382, game:226 | `64 + 2 * dC`. The right of the hub grows by dC (N->E goal starts at `x_s + C + sd + 2`, and x_s can be up to XH-2) and the hub moves right by dC, so the right side needs `2 dC` beyond the old width. With D109 clusters add the extra bridge sections (see below). | 64 |
| `XR` (right search reach, the "16") | `xO + 16` at Gen:206 | `16 + dC`; box max x = `xO + XR`. Must hold `x_s + C + sd + 2 + room` for the N->E goal window and the W->S starts, where x_s <= XH-2. Equivalent bound: `XR >= (XH - 2) + C + sd + 2 + room - xO`. | 16 |
| `XL` (left search edge) | `base.min = (1, 0)`, the `2` at Gen:263, 294 | 1 and 2 stay (frame left edge is 0, fixed). | 1, 2 |
| `X_WIN` (the "61") | 61 at Gen:272, 284, 294 | `W - 3` (kept as the same offset from the frame edge). Already clipped by the box, so it is not binding; keep it as W-3 for golden stability. | 61 |
| `X_START` (the "63") | 63 in Gen:225, 238, 252, 258, 301, and every `tile(63, w_row)` | `W - 1` | 63 |
| `N_SEARCH` (the "63" loop bounds p, q, t, s, k) | loops at Gen:318-382 | `W - 1` | 63 |
| `H_TERRAIN` (terrain rows) | `43`, origin y -2 at game:225-227 | unchanged in x/y direction by prio. Rows -2..(g + 20) + prio rows. Rows only need to grow if g grows; use `max_height + 3` where `max_height = g + 20` and keep 43 when g <= 20. | 43 |
| terrain width | `64 + 2*margin` | `W + 2*margin` | 64 + 2*margin |

D109 cluster note (not part of this recon's scope, but it sets W): each cluster needs its own bridge pair of up to 10 middle tiles plus `2 * bridge_margin` plus lane split/merge tails. If the clusters are spread across the whole E->S ... N->E distance, W also needs `+ n_clusters * (bridge span + 2*margin + lane tail)` or a loose bound `W = 64 + 2*dC + 16 * (n_clusters - 1)`. Since the cluster count is known only after pass 1, the frame should be built for the worst case (max prio_tl in the spec, `n_clusters <= 2` or 3), or pass 1 should run in a frame sized for the spec and pass 2 reuse it. Suggest: size the frame from the spec only (dC), and let D109 add a fixed extra per allowed cluster.

## 3. Implicit assumptions (not literal constants)

1. **Out-of-grid cells are unusable.** `JunctionTerrain::At` returns nullptr outside `width x height` and `UsableAt`/`SpanOk` treat that as blocked. If the generator frame grows but `MakeJunctionTerrain` does not, every candidate fails silently as "no layout" (no assertion). The two must be derived from the same `JunctionFrame`.
2. **Hub = anchor.** `XH` is baked into three independent sites that must agree: the generator (Gen:195, 558), the library fit (Gen:784), and the terrain snapshot (game:238). The tests also repeat it (see section 4).
3. **Lane programs are anchored to x = 63.** `RasterizeJunctionRoute({tile(63, w_row), X_NE, ...})`: the program moves are counted from the right edge, and the search loops p, q, t, s, k stop at 63. Lane B `p` is "the straight run from the frame edge to RFL", so `p` must be able to span from the edge to the S-bend; with a wider frame p's range must widen or the search won't find the head position. After the crop, the absolute start does not matter (the output depends on `h_e`), so the golden output is stable as long as the loop bounds merely grow. The first hit wins (p, q ascending), so a wider frame cannot change the answer at prio 1 provided the lower bounds are unchanged.
4. **Lane tails end at x = 0.** Gen:385 (`back().tile.x == 0`), Gen:368, Gen:418 (`raster_a ... x != 0`): lane merge / tail must reach exactly x = 0, which is the left frame edge and the E row start. This left edge stays at 0 in any wider frame (the extension is to the right of x = 0 and the hub moves right), so these are fine as long as XH, not the left edge, absorbs the left growth.
5. **The `61` windows are dominated by the box.** `PlainRowPieces(..., 61)` upper bounds are clipped by `base.max.x = xO + 16` (= 48 at bg 1), so at prio 1 the effective right limit is 48, not 61. Widening the box to `xO + XR` makes the 61 bound start to bind (at XR = 30 and beyond); `X_WIN = W - 3` must grow with W.
6. **Pass-1 and pass-2 share `base`.** Both use the same box, so XR serves both. Pass 2 forbids crossing (`crossable_w = false`) and fences the under-span tiles, which only live in `h_w+1..h_e-1`.
7. **The router grid pad is 12** (`junction_router.cpp:95-98`), independent of the frame: the box is the constraint, not the grid.
8. **`max_expansions = 50000`** was tuned for the 33 x 20 or so box; at XR + dC it grows roughly linearly in width. Raise it with the box or watch for silent `nullopt` at prio >= 3.
9. **The terrain's y extent (-2..40)** is a function of `height <= g + 20` (Gen sweep `first = g + 5`, `last = g + 20`, line ~676 and 735). It silently depends on g <= ~20; the prio rows below `yb` are added later by `PlaceJunctionSignals` and then checked against `terrain->UsableAt` (Gen:553-556), so a short terrain would fail only at that late check.
10. **Row -1 and row -2.** The frame uses y >= 0 for tracks; rows -2, -1 exist for the cluster/near-offset margin and tests (test blocks row -1 and checks that no track uses `hub.y - 1`). Not prio-dependent.
11. **Worst-case search cost.** Lane style 1 has nested p, q, t, s loops each to 63 with a rasterize call inside; the bounds are quartic in W in the worst case (early `continue` on errors prunes most). A wider frame raises the worst case; consider bounding p/q by the known h_e (p + q + span ≈ X_START - h_e) rather than W-1.

## 4. Risk list: tests and fixtures that hard-code the frame

`src/tests/junction_generator.cpp`:
- 491-498 `MakeFlatTerrain`: `origin {0, -2}`, `width 64`, `height 43` (used by 521, 536, 547, 552, 568, 624, 633, 642, 650, 657, 670, 721, 745, 767). Needs `MakeJunctionFrame`-derived size, or an overload taking the spec.
- 634 `SetTerrainRect(all, 0, 63, -2, 40, Blocked)` and 722 `SetTerrainRect(t, 0, 63, -1, -1, ...)`: hard-code 0..63 and -2..40.
- 609 `x = t.x - e.hub.x + 30`: hub x constant.
- 400-413 "a longer prio covers two train lengths": generates at prio_tl = 2 with no terrain; the D109 stop says prio >= 2 is blocked in the real pipeline, so verify whether this test currently passes (it is the natural first beneficiary).
- 513 golden JSON (`[.golden]`, env `JUNCTION_GOLDEN`): the acceptance test for "identical at prio 1". It is hidden by default; the refactor commits must run it before and after.
- 749-760 (bridge-span terrain test): derives positions from `base.hub`, so safe if hub is read from the result.

`src/tests/junction_game.cpp`:
- 166-190 `MakeJunctionTerrain: heights, steepness and class in the working frame`: asserts `width == 64`, `height == 43`, `origin == (0, -2)`; and uses `wx - 30` (172).
- 194-223: `terrain.At(35, 5)`, `At(38, 5)`, `At(30, 0)` (213, 222-223) are working coordinates relative to the hub.
- 319-320 `ox = 30 - near.result.hub.x + near.offset.x`: hub constant.
- `Map::Allocate(64, 64)` at 131-159 and `TileXY(64, 64)` anchor at 166: the anchor needs room for the frame on the map. At prio 1 the frame is unchanged, but a wider frame needs a larger test map or the snapshot's off-map cells become blocked (fine, but check the 166 test's map size).

Other tests (`junction_router.cpp`, `junction_gui.cpp`, `junction_build.cpp`, `junction_layout.cpp`, `junction_route.cpp`, `junction_station_rules.cpp`, `junction_design.cpp`, `junction_rules.cpp`, `junction_signal_pass.cpp`, `junction_train_model.cpp`): no frame constants found. The router tests build their own terrains (`FlatTerrain({0,0},{12,12})`) and `junction_gui.cpp` tests use only `Map::Allocate(64, 64)`.

Also the rig and `tools/ottd/layouts/*.json`: stored layouts are normalised, so independent of the frame.

## 5. Proposed order of changes (each keeps the golden identical at prio 1)

Before each commit: run the golden (`JUNCTION_GOLDEN=... junction tests`) and the `[junction]` tests; compare with the pre-refactor output.

1. **Introduce `JunctionFrame` with the current values, replace the literals (no behaviour change).** `struct JunctionFrame { int xh; int width; int right_reach; int height_rows; }` and `MakeJunctionFrame(spec)` returning 30 / 64 / 16 / 43 for every spec. Replace every literal in section 1a-1c by frame fields (`X_START = width - 1`, `X_WIN = width - 3`, loop bounds `width - 1`, `ox = frame.xh - e.hub.x`, hub = `frame.xh - shift`). Add `frame` as an argument of `MakeJunctionTerrain` (default = the constant frame) so the three sites share it. Update the comments. Tests: add a `MakeFlatTerrain(frame)` overload; golden must be byte-identical.
2. **Make the frame a function of the spec at prio >= 2.** `xh = 30 + dC`, `width = 64 + 2 dC`, `right_reach = 16 + dC`, with `dC = tl * (prio_tl - 1)`. At prio 1 `dC = 0`, so every number is unchanged. Thread the frame into the GUI/console callers (they pass spec and rotation; terrain must be taken with the spec's frame, so `junction_gui.cpp:469, 558` and `console_cmds.cpp:355` need the spec, or the frame computed by a shared helper). Raise `max_expansions` with the box if needed. Add tests: prio 2 and 3 with `MakeFlatTerrain(frame)` generate a layout (this is the D109 stop's precondition; D109 itself, the clustering, is still to finish).
3. **Re-apply the D109 WIP (clusters) on the new frame** (from `scratchpad/d109/d109-wip.patch`), adding the per-cluster extra width to `width`. The golden stays identical at prio 1 (one cluster).
4. (Optional, only if profiling shows a cost) bound the lane search loops p, q by the known head position instead of `width - 1`, and make the terrain snapshot lazy or cheaper; keep results identical.

## 6. One-line answers

- Fixed frame is in 21 places in the generator, 4 in the terrain snapshot, 3 doc comments, and about 15 test lines; the router, signal pass, checker, GUI and console are clean.
- Binding bound at prio >= 2 is `xO + 16` (not 61 or 63), plus `XH = 30` on the left; both grow by `dC = tl * (prio_tl - 1)`.
- The terrain and generator frames must come from one source, otherwise out-of-grid cells silently give "no layout".
