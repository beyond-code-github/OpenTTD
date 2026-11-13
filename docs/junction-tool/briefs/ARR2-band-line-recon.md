# ARR2: do the seeds obey the sweep's west-to-east line inside the bands? (recon for step 2, D254)

Read-only recon. Commit nothing; throwaway code only, reverted at the end. Keep your context small: send command output to files and report only what you found. You measure and report; you do not choose.

## Goal
Step 2 makes occupancy static: unrelated pieces are separated in x by an order the graph fixes, not by a flat solve. On rows the order is `order.rows[r]` (ARR1's row pairs; held at every seed). Inside the bands there is no such data yet. The order sweep (`OrderJunctionTrackGraph`, `src/junction/topology/junction_topology.cpp` ~555-700) keeps a working line `items` (edges west to east) and changes it only at band events: a crossing swaps two adjacent items (`std::iter_swap`), a node gathers its legs into one item.

Candidate rule (B1): two edges a and b that are adjacent in the line at some state of band k keep that west-to-east order on every tile row where both have tiles, between the event that made them adjacent and the event that ends their adjacency; a crossing event of a and b swaps them.

Static form (B2): per edge, the program tokens between two of its band events lie in the slice between those events (token order vs the crossings in `order.edge_crossings[e]`), so B1 can be written per token pair: token i of a and token j of b in the same slice, with a west of b there, means a's piece is west of b's piece.

## State
- Worktree `/home/pete/OpenTTD/.claude/nodes`: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-arr2 <HEAD given at launch>`. Rebuild `openttd_test` in `build-rel` (delete stale `.pch` files if the build complains). Leave untracked files alone.
- ARR0's probe (evaluation at seeds from rendered, tagged pieces): `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/arr0/arr0_test.cpp`. ARR1's merged test helpers `ArrangementTopology` / `CheckArrangementSeed` in `src/tests/junction/topology/junction_topology.cpp`.
- Output files go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/arr2/`.

## Measure, at the T, LL_RR x L_R and L_R x LL_RR seeds (`MakeJunctionGraphSeed`)
1. Instrument the sweep (throwaway) to emit, per band, the line before the band's first event and after each event (edge ids west to east; for a gathered node, name its item).
2. **B1:** for every pair adjacent in some line state, on every tile row (any y, including band rows and row stack y's) where both edges have tiles in that state's y-range (from the south event's y to the north event's y; use ARR0's event y), is the order held? Count held / overlapping (equal column allowed only where `JunctionTracksMayShareTile`) / reversed. List every reversed instance on T.
3. **B2:** for each band edge, assign each program token to a slice by its position relative to the edge's crossing tokens (Under/Over/Above/Tunnel) and its end nodes. At the seed, does every token's tile span lie within its slice's y-range (south event y to north event y, inclusive)? Count held / broken; list broken on T.
4. **Non-overlap:** count pairs adjacent in a line state whose pieces share no tile row at the seed (the constraint would bind pieces that never meet).
5. LL_RR x LL_RR (no seed; all-floor recording only): the number of line states and adjacent pairs, for sizing.

## Do not
- Commit, or change anything outside the throwaway probe.
- Run the gate or sweep; run only your probe with `timeout 600`.
- Touch any worktree other than nodes. No `git stash`, no `pkill` or broad `kill`, no game client, no background processes left.

## Report (short, about 35 lines)
- per seed: line states, adjacent pairs, B1 held / overlap / reversed, B2 held / broken, non-overlap count;
- every reversed or broken instance on T;
- LL_RR x LL_RR sizes;
- `git -C /home/pete/OpenTTD/.claude/nodes status --porcelain` (only pre-existing untracked files) and the kept probe path in the arr2 folder.

When you have reported, stop.
