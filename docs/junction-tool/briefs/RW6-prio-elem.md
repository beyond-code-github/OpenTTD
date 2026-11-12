# RW6: the prio element is a chain element on the Prio item's row (DESIGN-G1 §16.4 RW6; RECON-per-row-code.md §3, P1-P6)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes2 (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/nodes2 checkout -B wt2-rw6 <HEAD>` (given at launch). Never use git stash. Another agent works in /home/pete/OpenTTD/.claude/nodes (src/junction_levels.cpp ramp rule): never touch it. Use absolute paths.
- RW1-RW5 merged: tokens carry items, roles from predicates, one token-driven chain builder, points in chain order, stack from per-edge needs.
- The prio element today is no element: its tiles are decided twice. (a) Placement occupancy (~:1957-1981 at 7f5bc41e06; re-locate): for each row whose merge tile carries the main axis and a side half piece, row y = lane item row +- 1 by the JOINER's half-piece side (not the Prio item), X span njm..njm -+ even_prio, two Occupants keyed to the node. (b) The signal pass (~:3467-3567) re-derives the geometry from the joiner route's last tile and the tracks drawn, refusing "does not fit" if a row tile is occupied. Floors P2/P3 (Node->PrioNode = even_prio + 1, LaneMerge->PrioNode = even_prio + 2) apply on njm; P6 prio_beside ties the level solve's prio tiles to the main-line tile. Placement facts never see the prio tiles; only the level solve does.

## Read first (once each)
CODINGSTYLE.md (C++ part); RECON-per-row-code.md §3 and §4 step 7; DESIGN-G1 §16.4; src/junction_topology.{h,cpp}: Elem kinds, the chain builder, JunctionRunFloor/floor table, occupancy (P4), the facts block, the signal pass's prio drawing (P5), prio_beside (P6); src/junction_levels.cpp's use of prio_beside.

## Step 0 (findings, no code change)
On T and LL_RR seeds: for each prio element, the Prio item's row vs the row occupancy uses (lane +- 1 by the joiner's side): are they always equal? Its X span and the tiles/bits the signal pass draws. If they differ on any seed, report it (that is a hidden behaviour) and stop.

## Changes (after step 0)
1. A new Elem kind `Prio` on the chain of the edge that ends at the joiner merge (the out side of the merge node: say which edge you attach it to and why), index = the Prio item: Y pinned to the Prio item's row (`eq(0, ...)` to item_pt); X: last = njm, first = njm -+ even_prio along the row's heading; its floors become entries of the floor table (JunctionRunFloor) replacing P2/P3's special floors with the same values.
2. Occupancy (P4) emits the Prio Elem's Occupants from the Elem (its tiles and bits), replacing the node_bits derivation; facts see its tiles as a Straight group on its row (as any run).
3. The signal pass (P5) draws the prio element from the placement's Prio Elem tiles instead of re-deriving them; its "does not fit" check becomes unnecessary when facts/occupancy see the tiles (keep it as an assert-like failure with a reason if it can still trigger; say).
4. prio_beside (P6) reads the Prio Elem's tiles.

## Tests
1. **Outer (identity on flat):** `tools/junction/jt identity build-rel`; build-rel outputs of `"[topologysweep]"`, `"[topologystretch]"`, `"[placeoracle]"`, `"[topoprojsweep]"` equal to launch HEAD's (times stripped); LL_RR circuit byte-identical. `"[realterrainsweep]"` and `[junctiongame]` may change only where facts now see a prio tile on a blocked or unbuildable tile: report every changed fixture/offset class with one example tile; planned counts may rise or fall only for that reason (STOP otherwise).
2. **`[topoproj]` "topoproj: the prio element sees the terrain":** a house on a prio tile (not on any other track tile) of the T seed: the facts placement moves the prio (or fails with a reason naming the prio element), never places the prio on the house. Red: HEAD places it on the house and the signal pass refuses or draws over it (record).

## Stop conditions
- Step 0 finds a difference; a flat seed changes; a terrain change not explained by facts seeing a prio tile; anything not covered.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `'[topoproj]' 5`; `tools/junction/jt identity build-rel`; the sweeps; `tools/junction/jt gate build-rel` (count, time). TESTS.md rows.
- One commit: `Change: Junction prio element is a chain element on its own row, seen by facts and occupancy`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus step 0's findings, the attachment edge, and any terrain changes. When you have reported, stop.
