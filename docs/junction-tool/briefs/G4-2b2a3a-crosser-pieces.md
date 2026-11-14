# G4 2b2a3a: a crosser's piece at each item row, with affine columns and guarded O1 rows (DESIGN-G4 §1 A6, §3 O1; D271)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
2b2a2 records each stack item row's occupants in a static order with one O1 row per unrelated pair (`system.item_rows`, `system.static_rows`). A band-edge crosser's tiles are still recorded at the bend tile that covers the row at the recording, so their columns hold only while the rows stay where they are.

This step makes the crosser static. For each (crosser, item row), the recording lists the candidate pieces of the crosser's chain that may cover the row. Each candidate has:
- its row conditions, as y rows;
- its tiles' columns, affine in a point's column and the rows (`x[p] + c0 + cy (y[item] - y[p])`, no L term);
- its O1 rows, which hold only under its choice.

The step also closes D269's hole: O1 gap 0 is allowed only between two band-edge pieces.

Nothing reads any of this yet. No layout, objective, hash or digest moves.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`: run `git -C /home/pete/OpenTTD/.claude/ng1 checkout -B wt-2b2a3a <HEAD given at launch>`. Leave its untracked files alone.
- `build` (Debug) and `build-rel` (Release) exist. Delete stale `.pch` files if the build complains.
- Output files go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/2b2a3a/`.
- A GUI game may be running from `ng1/build-rel/openttd`. Build only the `openttd_test` target, and never touch that process.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md` (C++ part).
- `docs/junction-tool/DECISIONS.md` rows D269 and D271 (grep `^| D2(69|71)`).
- `docs/junction-tool/RECON-A6-crossers-fans.md`, §1 to §3 (lines 1-75). These are the facts this brief rests on: 48 (crosser, item row) pairs, exactly one cover each, and the closed forms of §2.
- `src/junction/junction_track_geometry.h/.cpp`: `BendRasterOffsets`, `JunctionBendTileForms`, `BendColumnForm` (lines 26-130). The new helper follows `BendColumnForm`'s style of reading rasters at a large L.
- `src/junction/placement/junction_placement.h` lines 135-200 (`JunctionSystemColumn` and the item-row types).
- `src/junction/placement/junction_placement.cpp`:
  - 215-240 (`Elem`: `tok`, `absent`);
  - 925-935 (`next_present`);
  - 1324-1335 (`bend_bits`);
  - 1336-1470 (`Occupant`, `related`, `add_bend`, the band-edge chain loop with the vertical tiles at ~1462);
  - 1540-1615 (the item-row record block).
- `src/tests/junction/topology/junction_topology.cpp` lines 5880-6030 (`RecordItemRowSeed`, `ItemRowColumn`, `CheckItemRowSeed`, the three item-row tests).
- `src/tests/junction/junction_track_geometry.cpp` (the Corner column form test, as a template).

## Geometry (`junction_track_geometry.h/.cpp`)
```cpp
/** One tile column of a bend on a range of the rows it travels: at r rows from its first tile, the tile's column is c0 + slope r from the first tile's. */
struct JunctionBendRowTile { int c0; int slope; TrackBits bits; };

/** The rows r = lo0 + lo1 L .. hi0 + hi1 L travelled from a bend's first tile on which the bend has the same tile columns. */
struct JunctionBendRowSegment { int lo0, lo1, hi0, hi1; std::vector<JunctionBendRowTile> tiles; };

/** A bend's tiles by the rows it travels (DESIGN-G4 O1): row y = y_first + d r. */
struct JunctionBendRows { int d; std::vector<JunctionBendRowSegment> segments; };

/** The rows of the bend of length L (an S-bend: 2L halves; a Corner: 2L + 1) starting on the given heading with the given hand, read from the rasters. */
JunctionBendRows JunctionBendRowForms(int heading, int hand, bool corner);
```
**How it is computed.**
- **d and slope.** Use `VX`/`VY` as in `BendColumnForm`, with `side = (heading + hand + 4) % 4`:
  - vertical heading (0, 2): `d = VY[heading]` and `slope = VX[side]`;
  - horizontal heading (1, 3): `d = VY[side]` and `slope = VX[heading]`.
- **Tiles per row at L = 64.** Rasterise as `BendRasterOffsets` does, but keep each piece's track: union the tracks of the halves on one tile. For each tile, `r = d * dy` and its key is `(dx - slope * r, bits)`.
- **Segments.** Group rows into maximal runs of consecutive r that have the same set of keys. Each run is a segment, and its tiles are the keys in ascending c0.
- **lo and hi as forms in L.** Do the same at L = 63 and match segments in order, then take `lo1 = lo(64) - lo(63)` and `lo0 = lo(64) - 64 lo1`, and the same for hi.
- **Stop if the groupings disagree.** If the segment count or keys differ between 63 and 64, return `{0, {}}`; the test then fails, so stop and report it.

**Expected,** from RECON-A6 §2:
- **vertical S-bend:** one segment, rows 0..L-1, two tiles;
- **vertical Corner:** rows 0..L-1 with two tiles, then the last row L with one tile; the key may merge with a range, and that is fine;
- **horizontal S-bend:** r = 0 one tile, 1..L-1 two tiles, L one tile.

## Data (`src/junction/placement/junction_placement.h`)
- **`JunctionStaticRow`** gains two members after `reason`:
  ```cpp
  std::vector<std::pair<int, int>> y; ///< (point, coefficient), each adding coefficient * y[point] to the left side; points unique, coefficients non-zero.
  std::vector<int> pieces; ///< The crosser pieces it holds under (indexes of JunctionPlacementSystem::crosser_pieces), all chosen; empty: always.
  ```
  Its comment becomes: `x[v.pt] + v.c0 + v.c1 L(v.bend) - (x[u.pt] + u.c0 + u.c1 L(u.bend)) + sum of y >= gap; a column with pt -1 has no x term.`
- **`JunctionItemRowOccupant`** gains, after `kind`:
  ```cpp
  int piece = -1; ///< A crosser's candidate piece (index of crosser_pieces), -1 for every other occupant.
  int cy = 0; ///< Both columns also add cy (y[ya] - y[yb]).
  int ya = -1, yb = -1;
  ```
- **New, before `JunctionPlacementSystem`:**
  ```cpp
  /** A candidate piece of a band-edge crosser at a stack item row it crosses (DESIGN-G4 A6): a piece of its chain around its Under tokens of that row; exactly one of each (edge, item) covers the row. */
  struct JunctionCrosserPiece {
  	uint32_t edge; ///< The crosser.
  	int item; ///< The stack item whose row it may cover.
  	int bend; ///< Its bend's id, -1 for the vertical run.
  	int segment; ///< The segment of JunctionBendRowForms, -1 for the run.
  	int pt; ///< The point its columns and rows are measured from: the bend's first point, or the last point of the element above the run.
  	bool chosen; ///< It covers the row at the recording.
  };
  ```
- **`JunctionPlacementSystem`** gains, after `item_row_share`:
  ```cpp
  std::vector<JunctionCrosserPiece> crosser_pieces; ///< The crossers' candidate pieces at item rows, grouped by (edge, item) in order.
  int crosser_overlap = 0; ///< (edge, item) groups where more than one candidate covers the row at the recording.
  int crosser_uncovered = 0; ///< (edge, item) groups where none does.
  ```
  Also update the `item_row_skipped` comment: "Occupants of item rows left to 2b2a3b (another row's port fan)".

## Changes (`src/junction/placement/junction_placement.cpp`, the item-row record block)
1. **Lifecycle.** Add `recorded_crosser_pieces`, `recorded_crosser_overlap` and `recorded_crosser_uncovered` next to `recorded_item_rows`. Clear them and copy them into `now` exactly as `recorded_item_rows` is cleared and copied.
2. **Drop the point-bound crosser occupants.** In the ranking loop, where an occupant becomes `"crosser"` (~1589), skip it without counting it in `item_row_skipped`. Every other branch stays.
3. **Candidates.** Do this once per X record, before the loop over `by_row`. For each stack item i (row `r = structure.stack[i].row`), and each crosser x of row r (the edge items of `travel[r]` strictly between its first and last entries), with `D = travel_pos(r, false, x)`:
   - **Under tokens.** These are x's program tokens of kind Under whose index is `leg_edge[r]`. If there are none, STOP and report (x, r).
   - **The elements around them.** In `chain[x]`, take:
     - a: the last element with `!absent` and `tok < ` the first Under token. The chain's start Tile counts as before every token: if its `tok` is -1, treat it as -1.
     - b: the first element with `!absent` and `tok >` the last Under token. The end Tile counts as after every token: if its `tok` is -1, treat it as `INT_MAX`.
     - If a present element lies strictly between a and b, STOP and report.
   - **Candidates, in this order:**
     - **a's segments,** when a is a Corner, SBend or Fan. Read them from `JunctionBendRowForms(a.hin, a.hand, a.k == Corner)`, with `bend = bend_ids.at(&a)`, `segment = s` and `pt = a.first`.
     - **The run,** when `VX[a.hout] == 0` and `a.last != b.first`, with `bend = -1`, `segment = -1` and `pt = a.last`. If `VX[a.hout] != 0`, STOP and report.
     - **b's segments,** as for a, with `pt = b.first`.
   - **The row conditions of each candidate n,** as static rows with `pieces = {n}`, no x term (`v = {-1}`, `u = {-1}`), and reason `"A6 e<x> item <i> piece <n>: <run | bend <id> seg <s>> <lo | hi>"`. Let `Y = item_pt[i]`:
     - **Run,** with `Dy = VY[a.hout]`:
       - `Dy (y[Y] - y[a.last]) >= 1`, that is `y = {(Y, Dy), (a.last, -Dy)}` with gap 1;
       - `Dy (y[b.first] - y[Y]) >= 1`.
     - **Bend segment,** with d from the forms:
       - `d (y[Y] - y[pt]) - lo1 L >= lo0`, that is `v = {-1, 0, -lo1, lo1 != 0 ? id : -1}` with gap lo0;
       - `hi1 L - d (y[Y] - y[pt]) >= -hi0`, that is `v = {-1, 0, hi1, hi1 != 0 ? id : -1}` with gap `-hi0`.
   - **chosen.** A candidate is chosen when both its rows hold at the recording's rows (`vy`) and lengths: the bend's length as `record_system` gives it, `m`. In each (x, i) group, the first candidate that holds is chosen. Count more than one holding in `crosser_overlap`, and none holding in `crosser_uncovered`.
   - **The candidate's occupants on item i's row.** Kind `"crosser"`, rank `100 * D`, edge x, copy 0, node `UINT32_MAX`, footprint 0, piece n, `ya = Y`, `yb = pt`. Give each a step after every by_row occupant of that row, in candidate order.
     - **Run:** one occupant, columns `{a.last, 0}` both, `cy = 0`, bits `Track::Y`.
     - **Bend segment:** one occupant per tile t, columns `{pt, t.c0}` both, `cy = t.slope * d`, bits `t.bits`.
4. **Order and emit.** Merge the candidates' occupants into `ranked` for item i before the sort. Two occupants of one crosser are related: same edge, copy 0.

   For each unrelated pair a before b, the static row is:
   - `v = b.west`, `u = a.east`;
   - `y` = b's `(ya, +cy)`, `(yb, -cy)`, plus a's `(ya, -cy)`, `(yb, +cy)`, merged by point with zero coefficients dropped;
   - `pieces` = the non-negative `piece` of a and of b.

   The empty-run test (`flat_right < flat_left`) applies only to occupants with `piece < 0`.
5. **The gap.** Use `gap = (g.share && band(a) && band(b)) ? 0 : g.apart`, where `band(o)` is `o.kind == "node band" || o.kind == "crosser"`. `item_row_share` counts only the gap-0 rows.
6. **CLAUDE.md** (placement, standalone facts, no D-ids or design references). Replace the item-row fact (the last line) with:
   > A recording lists each stack item row's occupants west to east in a travel order read from the graph, the order, the structure and the programs, and one static row per unrelated pair on that row: gap 0 only when both are band-edge pieces that may share a tile, else the least column gap. A band-edge crosser stands on each item row of a row it crosses as one of its chain's pieces around its Under tokens of that row (the bend before, the vertical run, the bend after), each candidate with the rows it covers and its tile columns affine in its point's column and the rows; the candidate's static rows hold only when it is the one chosen. The solver does not read them yet.

   Add to the root `src/junction/CLAUDE.md`, under the root files' facts:
   > A bend's tiles on the rows it travels are segments of rows, each tile's column moving by a fixed slope per row (`JunctionBendRowForms`).

## Tests
Red first: add the members and a stub `JunctionBendRowForms` returning `{0, {}}`, build, and see each new test fail on an assertion. Record each failure message.
1. **`src/tests/junction/junction_track_geometry.cpp`, "bend row forms reproduce every bend raster" `[junction]`.** For heading 0..3, hand ±1, both kinds and L 1..9, the tiles that the forms give must equal the raster's tiles:
   - the forms' tiles are, per segment and per r in `[lo0 + lo1 L, hi0 + hi1 L]` when non-empty, per tile, `(c0 + slope r, d r)` with the tile's bits;
   - the raster's tiles are `JunctionBendTileForms`' offsets at L, plus each tile's bits from the rasteriser, as `BendRasterOffsets` reads it.

   Compare them as sets of (dx, dy, bits). Also pin the vertical S-bend heading 2, hand -1: d = +1, one segment `{0, 0, -1, 1}` with tiles c0 0 and 1, slope +1.
2. **`junction_topology.cpp`, update `CheckItemRowSeed`.**
   - `ItemRowColumn` takes `pt == -1` as no x term.
   - A row's value adds `sum coef * solved.y[pt]`.
   - A row is checked only when every piece in `pieces` is chosen.
   - The WARN adds the rows skipped as unchosen, `crosser_pieces.size()`, `crosser_overlap` and `crosser_uncovered`.
   - The touch counter matches occupants by columns and `piece`.
3. **"crossers: one piece covers each crossed item row at the seeds" `[junction][topology]`** (T and LL_RR x L_R; L_R x LL_RR goes in `[topologysweep][.sweep]` as a separate case).
   - The number of (edge, item) groups is T 6, LL_RR x L_R 30, L_R x LL_RR 12 (RECON-A6 §1).
   - Every group has exactly one chosen piece, and `crosser_overlap == 0` and `crosser_uncovered == 0`.
   - At `solved`, exactly one candidate per group has both condition rows holding, and it is the chosen one.
4. **"crossers: a chosen piece's columns are the crosser's tiles"** (same hubs and split).
   - For each chosen candidate, evaluate its occupants' columns at `solved` (with y terms).
   - The oracle is the crosser's tiles on that row, computed in the test from `system.chains` and `system.bends`:
     - bend tiles at `solved.x/y[first] + JunctionBendTileForms(hin, hand, m, kind)` offsets;
     - vertical-run tiles at `x[a.last]` for rows strictly between.
   - The two must be equal as column sets. The recon found 65 of 65 tiles.
5. **"item rows: gap 0 only between band-edge pieces"** (T, LL_RR x L_R). Every static row with gap 0 comes from a pair whose two kinds are both "node band" or "crosser": match the reason's kinds. Each hub has at least one gap-0 row: LL_RR x L_R has the e8/e11 shares.
6. **Unchanged and green:** "item rows follow the arrangement's row order" (a crosser maps to its edge, all candidates alike) and "item rows: the order needs no point".

## Do not
- Feed any row to the joint, the loop or any solve. Change no existing relation, clash or digest. Do not touch `add_occupancy`'s flat sort or clash path: today's crosser occupants stay there.
- Add a binary variable, O2 or fan rows, clearance, or O3 rows (2b2a3b and 2b2b).
- Touch any worktree but `ng1`, or the running game. `.claude/nodes2` is reserved for the engineer's game.
- Use `pkill`, a broad `kill`, or any form of `git stash`.

## Stop conditions
- A crosser of a row has no Under token under that row's leg.
- A present element lies between a and b.
- A run is horizontal.
- `JunctionBendRowForms` disagrees between L 63 and 64, or with a raster.
- A seed group is uncovered or overlapping.
- A chosen piece's columns differ from the oracle.
- A row fails at a seed.
- Any identity value moves, or any existing test fails.
- The gate or sweep goes over budget because of this step. Compare against a run at the launch head on the same machine state, and report both times.

## Gates (D264)
Run `tools/junction/jt gate build-rel`, `tools/junction/jt sweep build-rel` and `tools/junction/jt identity build-rel`, each with a Bash timeout of at least 400000 ms. All must pass; report cases and times.

Make one commit on wt-2b2a3a: `Add: Junction recording lists each crosser's candidate pieces at item rows`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
- The references read; the commit; the red evidence per test.
- The types and members added.
- Per hub: candidate groups, candidates, chosen, overlap and uncovered; static rows (total, piece-guarded, condition rows, gap 0); and rows skipped as unchosen.
- Gate, sweep and identity: cases and time. Any stops. `git status --porcelain` and `git log -1`.

When you have reported, stop.
