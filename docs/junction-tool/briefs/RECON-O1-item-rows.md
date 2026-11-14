# Recon O1: what stands on each stack item row today, and how it maps to the arrangement (read-only; D268)

You are a read-only recon. You change no committed file. Throwaway probe code is allowed in your worktree and must be reverted before you finish (save it as a patch). Keep your context small: send command output to files, read with offset and limit, and report only conclusions.

## Goal
The next step (2b2a2) adds DESIGN-G4's O1 rows: on each stack item row, the occupants in a fixed west-to-east order, each pair separated by a static row (no binary, no point). Before it can be briefed, the orchestrator needs facts about today's code and the three seeds. Write them to `/home/pete/OpenTTD/docs/junction-tool/RECON-O1-item-rows.md`, short and factual. Mark anything you inferred rather than read or ran as INFERRED.

## State
- Worktree `/home/pete/OpenTTD/.claude/o4c2`: `git -C /home/pete/OpenTTD/.claude/o4c2 checkout -B wt-recon-o1 <HEAD given at launch>`. It has `build-rel` (Release, stale: rebuild `openttd_test` there; delete stale `.pch` if the build complains). Output files go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/recon-o1/`.
- Other agents work in `.claude/ng1` and `.claude/nodes`. Touch no worktree but `o4c2`; `.claude/nodes2` is reserved for the engineer's game.

## Read first
- `docs/junction-tool/DESIGN-G4-static.md` lines 54-70 (A5, A6), 71-94 (the family table), 104-127 (O1, O2, O3);
- `src/junction/placement/junction_placement.h` lines 36-240 (the recording types);
- `src/junction/topology/junction_topology.h` lines 270-300 (`JunctionTrackArrangement`; `JunctionArrangementUnder` above it);
- `src/junction/placement/junction_placement.cpp`: `add_occupancy` (~1306-1520), `hug_heads` (~1519-1560), `row_pieces`, the chain builder for band edges and row edges, and how `item_pt`, `node_pt` and the row chains' points are made (use the LSP or grep to find them).

## Questions (answer each, with jp line numbers)
1. **Sources.** For a stack item row y, list every kind of occupant `add_occupancy` puts on it: node tiles, row chain pieces (Tile, Run, Bend), band-edge chain elements (Tile, Heads, Corner, SBend, Fan, vertical tiles between elements), prio footprint tiles. For each say how its west and east columns are written (point + offset, bend tile forms with L) and which point variables it uses.
2. **Item row vs other rows.** Are the item rows exactly the distinct `vy[item_pt[i]]`? Does any non-item row also hold row-chain pieces (S-bends between items, copies)? Name them.
3. **At the three seeds** (T, LL_RR x L_R, L_R x LL_RR; build them as the ARR1 tests do: `ArrangementTopology` and `MakeJunctionGraphSeed` in `src/tests/junction/topology/junction_topology.cpp` ~5394-5540; record with `RecordJunctionPlacementSystem` and take the flat solution), a throwaway probe dumps per item row the occupants west to east at the seed: kind, edge, copy, node, columns. Save the dump. Then answer:
   - (a) Does the seed's occupant order on each item row follow `arrangement.row_pairs` (items west to east) once each occupant is mapped to its item? Count mismatches.
   - (b) Which occupants have no item in ARR1: band-edge crossers, heads of doubled bridges, prio footprint, others? Per hub, how many per kind?
   - (c) For each band-edge crosser of an item row: which chain element covers that row at the seed (vertical tile between elements, a run, a bend's tail or head, a Heads tile)? Compare with `arrangement.unders` (the candidates per Under token) where the row is a doubled bridge's row.
   - (d) Pairs of unrelated occupants on one item row at gap 0 (sharing a tile) at the seeds: how many, which kinds.
4. **The hug.** How `hug_heads` finds the heads and crossers of each doubled bridge, and which relations it adds (which of them are `order_pin`). How the occupancy pairs of the heads and crossers are separated today.
5. **The static order of a row's occupants.** Propose, from what you found, a rule that orders every occupant of an item row from the graph, order, programs and arrangement only (no point), and check it against the three seeds' dumps: count any occupant pair the rule orders differently from the seed. Do not write the rule as code beyond the probe.

## Do not
- Commit anything; leave `o4c2` clean (`git status --porcelain` empty apart from untracked files that were there) on `wt-recon-o1` at the launch head.
- Use `pkill`, a broad `kill`, or any form of `git stash`.

## Report (short)
- the recon file's path and a five-line summary (answers to 3a, 3b, 3c, 5 in numbers);
- the probe patch's path;
- `git -C /home/pete/OpenTTD/.claude/o4c2 status --porcelain` and `git log -1`.

When you have reported, stop.
