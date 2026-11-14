# G4 2b2a2: item-row occupants in a static order, and O1 rows (DESIGN-G4 §1 A5, §3 O1; D268, D269)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
Today the occupants of a row are ordered by the flat solution's columns, so the order is taken at a point. The static model (DESIGN-G4) orders each stack item row's occupants from the graph, the order, the structure and the programs alone, and separates every unrelated pair by a static row.

This step adds that order (A5) and those rows (O1) to the recording as data, built in the same pass that builds today's occupancy. Nothing reads them yet: no layout, objective, hash or digest moves. The scope is ruled in D269: read it.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`: `git -C /home/pete/OpenTTD/.claude/ng1 checkout -B wt-2b2a2 <HEAD given at launch>`. Leave its untracked files alone. `build` (Debug) and `build-rel` (Release) exist; delete stale `.pch` files if the build complains.
- Output files go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/2b2a2/`.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md` (C++ part);
- `docs/junction-tool/DECISIONS.md`, rows D268 and D269 (grep `^| D26[89]`);
- `docs/junction-tool/RECON-O1-item-rows.md` (whole, 88 lines; §1 lists the occupant sources with line numbers, §5 is the rank rule you implement);
- `docs/junction-tool/wip/o1-item-rows-probe.patch`, lines 1-135 (the dump hook: how the probe found each piece's chain, element, token, role and the number of HeadsPairs before it) and 200-280 (how the probe computed q). It is a reference, not code to copy;
- `src/junction/placement/junction_placement.h` lines 36-240 (the recording types);
- `src/junction/placement/junction_placement.cpp`: 480-520 (`recorded_clashes`, `record_system`), 1165-1240 (`RowPiece`, `row_pieces`), 1306-1475 (`add_occupancy`), 2125-2135 (where `recorded_clashes` is cleared);
- `src/junction/junction_track_geometry.h` (`JunctionUnrelatedTileGap`);
- `src/tests/junction/topology/junction_topology.cpp` lines 5394-5530 (`ArrangementTopology`, `CheckArrangementSeed`, the ARR1 tests: your template for building the seeds).

## Data (`src/junction/placement/junction_placement.h`, before `JunctionPlacementSystem`)
```cpp
/** One occupant of a stack item row in the static model (DESIGN-G4 §1 A5): a piece of today's occupancy family on that row, with its travel rank. */
struct JunctionItemRowOccupant {
	JunctionSystemColumn west; ///< Its west column.
	JunctionSystemColumn east; ///< Its east column (west - 1 for an empty run).
	TrackBits bits; ///< The tracks of its tiles.
	uint32_t edge; ///< The graph edge, UINT32_MAX for a node's tile or a prio footprint.
	int copy; ///< 0 or 1, -1 for the part both copies share.
	uint32_t node; ///< The node of a node's tile or of a prio footprint, else UINT32_MAX.
	int footprint; ///< A prio footprint's travel sign, 0 for tracks.
	int rank; ///< 100 times its travel position on its row (RECON-O1-item-rows §5).
	int step; ///< Its index in the order the occupancy family adds the row's pieces; breaks ties of rank.
	std::string kind; ///< "node", "node band", "in", "out", "leg run", "lane split", "lane merge", "heads", "prio" or "crosser".
};

/** A static row of the placement model (DESIGN-G4 §3): x[v.pt] + v.c0 + v.c1 L(v.bend) - (x[u.pt] + u.c0 + u.c1 L(u.bend)) >= gap. */
struct JunctionStaticRow {
	JunctionSystemColumn v;
	JunctionSystemColumn u;
	int gap;
	std::string reason; ///< "O1 item <i>: <kind a> e<edge> / <kind b> e<edge>".
};
```
`JunctionPlacementSystem` gains, after `clashes`:
```cpp
	std::vector<std::vector<JunctionItemRowOccupant>> item_rows; ///< Per stack item, the occupants of its row west to east (DESIGN-G4 A5), from the last X round whose rows were known.
	std::vector<JunctionStaticRow> static_rows; ///< The O1 rows of item_rows (DESIGN-G4 §3).
	int item_row_skipped = 0; ///< Occupants of item rows left to 2b2a3 (another row's port fan, a tile whose row moves with its bend's length).
	int item_row_share = 0; ///< O1 pairs that may share a tile and so have gap 0.
```

## Changes (`src/junction/placement/junction_placement.cpp`)
1. **Lifecycle.** Copy `recorded_clashes` exactly. Add `recorded_item_rows`, `recorded_static_rows`, `recorded_item_row_skipped` and `recorded_item_row_share` next to it. Clear them where it is cleared (~2131), and copy them into `now` where it is copied (`if (phase == 0)` in `record_system`).
2. **Shared helpers.** Move the `Occupant` struct, `at_node` and `related` out of `add_occupancy` to just before it, unchanged, so that the new code uses the same relatedness.
3. **Collect.** Inside `add_occupancy`, when `record` is true, after the occupants are collected into `by_row` (after the prio footprint block, ~1431) and before the per-row sort:
   - **Item of each row.** `item_of_row[y]` is the stack item i with `vy[item_pt[i]] == y`. If two items share a row, STOP and report the hub and rows. Rows that are not item rows are skipped entirely.
   - **The row r of item i** is `structure.stack[i].row`. Its order items are `order.rows[r]`: a first node s, the crossers, a last node m. Let k = items - 1. The travel position of an order item is its index, counted from the In edge's side: reverse `order.rows[r]` when the row travels west (`east[r]` false). `pos(x)` is that index for crosser edge x.
   - **Rank each occupant on the item row** by RECON-O1 §5's table, times 100. Use integers: -100, 0, 5, 10, `100 pos - 50`, `100 pos - 40`, `100 pos`, `100 pos + 40`, `100 pos + 50`, `100 k - 55`, `100 k - 50`, `100 k - 40`, `100 k - 25`, `100 k`, `100 k + 100`. The kinds:
     - **"node":** a node's tile.
     - **"node band":** a band edge incident to the row's s or m, ranked as that node.
     - **"in" / "out":** pieces of the row's own In or Out edge.
     - **"leg run", "lane split", "lane merge":** the leg's runs and S-bends by role. A shared run has copy -1: before the lane split, or after the lane merge.
     - **"heads":** a HeadsPair tile. Bridge j is the leg's j-th bridge in `structure.bridges` order. The west or east head is the pair's first or last tile in travel order.
     - **"prio":** footprint tiles.
     - **"crosser":** a band edge that is not incident to s or m and is one of the row's order items, ranked `100 pos(x)`.
     For which bridge a head belongs to, and which role an S-bend has, read the chains as the probe did: the count of HeadsPair elements before the piece in its chain, and the S-bend element's role. Thread that information onto `Occupant` with new fields (`int rank`, `std::string kind`) that the existing code ignores. Do not compute ranks from flat columns.
   - **Skipped,** each counted in `recorded_item_row_skipped`:
     - a piece of another row's In or Out edge (a port fan crossing this row);
     - a band-edge tile from `add_bend` whose `JunctionBendTileForm` has `dy1 != 0` (its row moves with L);
     - any band-edge occupant on an item row that is neither "node band" nor "crosser". Count it under its own heading in the report.
   - **Order and emit.** For each item row: stable-sort the ranked occupants by (rank, step). Reverse the order when the row travels west, so that `item_rows[i]` is west to east. Then, for every pair a before b (west to east) that `related` does not relate, and where neither is an empty run (east before west):
     - `JunctionTileGap g = JunctionUnrelatedTileGap(a.bits, b.bits)`;
     - `gap = g.share ? 0 : g.apart`, and `item_row_share++` when share;
     - push `JunctionStaticRow{b.west, a.east, gap, reason}`.
     The columns are the occupants' recorded `left` and `right` (`JunctionSystemColumn`), as `recorded_clashes` uses them. A footprint pair is ordered by rank like any other.
   - Equal (rank, step) for two unrelated occupants is a tie. STOP and report the kinds.
4. **CLAUDE.md** (placement; standalone facts, no D-ids or design references): "A recording lists each stack item row's occupants west to east in a travel order read from the graph, the order, the structure and the programs (node, its In or Out pieces, the leg's runs and S-bends by role, each bridge's heads around its crossers, the prio footprint), and one static row per unrelated pair on that row, its gap 0 when the two may share a tile, else the least column gap. The solver does not read them yet."

## Tests (red first: add the members and leave them empty, build, see each test fail on an assertion, record each message)
In `src/tests/junction/topology/junction_topology.cpp`, after the arrangement tests, building each seed and its recording as `CheckArrangementSeed` does. Record with `RecordJunctionPlacementSystem` at the seed's own choices, and take the point from the recording's `solved` (REQUIRE it non-empty). L of bend b is `bends[b].m`.
1. **"item rows: every O1 row holds at the seeds"** `[junction][topology]` (T, LL_RR x L_R), plus the same body for L_R x LL_RR in `[junction][topologysweep][.sweep]` (as the ARR1 L_R x LL_RR case is split):
   - `static_rows` is non-empty;
   - every stack item has a non-empty `item_rows` entry;
   - every row holds at the point: `x[v.pt] + v.c0 + v.c1 L - (x[u.pt] + u.c0 + u.c1 L) >= gap`. On failure, INFO the row's reason and both sides;
   - WARN per hub the occupants, rows, `item_row_skipped`, `item_row_share`, and the count of share rows whose two tiles would touch at gap 1 (`JunctionUnrelatedTileGap(...).apart == 2`).
2. **"item rows follow the arrangement's row order"** `[junction][topology]` (T, LL_RR x L_R): map each occupant with an item to its `JunctionTrackRowItem`:
   - a node or node-band occupant to its node;
   - a crosser to its edge;
   - "in" to s, "out" to m.
   Then, along `item_rows[i]` west to east, the mapped items never go backwards in `arrangement.row_pairs` order for that row. The recon found 0 reversals.
3. **"item rows: the order needs no point"** `[junction][topology]` (T): recording the T seed's choices twice, at its own solution and at the seed point shifted by +3 columns for every point (`at` argument), gives equal `item_rows` (kinds, ranks, columns) and `static_rows`.

## Do not
- Feed the rows to the joint, the loop or any solve, or change any existing relation, clash or digest.
- Add a binary, a y term, a piece choice, a fan row, or anything of O2/O3 (2b2a3 and 2b2b).
- Touch any worktree but `ng1`; `.claude/nodes2` is reserved for the engineer's game.
- Use `pkill`, a broad `kill`, or any form of `git stash`.

## Stop conditions
- Two stack items share a row, an unrelated tie, or a row that fails at a seed. Report it with its reason; do not change the rank rule.
- Any identity value moves, or any existing test fails.
- The gate or sweep goes over budget because of this step: compare against a run at the launch head on the same machine state, and report both times.

## Gates (D264)
- `tools/junction/jt gate build-rel`, `tools/junction/jt sweep build-rel` and `tools/junction/jt identity build-rel`, each with a Bash timeout of at least 400000 ms: all pass; report cases and times.

One commit on wt-2b2a2: `Add: Junction recording lists item-row occupants and their static rows`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
- references read; the commit; red evidence per test;
- types and members added, and the fields threaded onto `Occupant`;
- per hub: occupants, rows, skipped (by heading), share, and share-that-touch;
- gate, sweep and identity: cases and time; stops; `git status --porcelain` and `git log -1`.

When you have reported, stop.
