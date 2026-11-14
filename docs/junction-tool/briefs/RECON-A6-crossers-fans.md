# Recon A6: crossers' pieces at item rows, and other rows' port fans (read-only; D271)

You are a read-only recon. You change no committed file. Throwaway probe code is allowed in your worktree and must be reverted before you finish (save it as a patch). Keep your context small: send command output to files, read with offset and limit, and report only conclusions.

## Goal
2b2a2 recorded each stack item row's occupants in a static order with O1 rows (`system.item_rows`, `system.static_rows`). Two kinds of occupant are still at the recording's point: a band-edge crosser's tile is recorded at the bend tile index that covers the row at the recording (its column is right only while the rows stay), and another row's W/E port fan crossing the row is skipped. The next step (2b2a3a, then 2b2a3b) makes both static: a crosser gets a piece choice per item row with an affine column in x, L and y (DESIGN-G4 §3 O1, P3 generalised), and the fans get O2 rows with a side sigma. Before it can be briefed, the orchestrator needs the facts below. Write them to `/home/pete/OpenTTD/docs/junction-tool/RECON-A6-crossers-fans.md`, short and factual. Mark anything you inferred rather than read or ran as INFERRED.

## State
- Worktree `/home/pete/OpenTTD/.claude/o4c2`: `git -C /home/pete/OpenTTD/.claude/o4c2 checkout -B wt-recon-a6 <HEAD given at launch>`. It has `build-rel` (Release, stale: rebuild `openttd_test`; delete stale `.pch` if the build complains). Output files go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/recon-a6/`.
- Other agents may work in `.claude/ng1`. Touch no worktree but `o4c2`; `.claude/nodes2` is reserved for the engineer's game.

## Read first (once each)
- `docs/junction-tool/DESIGN-G4-static.md` lines 54-70 (A5, A6), 104-118 (O1, O2), 150-178 (variables, tight M);
- `docs/junction-tool/RECON-O1-item-rows.md` (whole; §3c is where this recon starts);
- `docs/junction-tool/wip/o1-item-rows-probe.patch` (the previous probe; reuse its dump hook and hidden test);
- `src/junction/placement/junction_placement.h` lines 36-200;
- `src/junction/topology/junction_topology.h` lines 260-300 (`JunctionUnderPiece`, `JunctionArrangementUnder`);
- `src/junction/junction_track_geometry.h/.cpp` (`JunctionSBendColumnForm`, `JunctionCornerColumnForm`, `BendColumnForm`, `JunctionBendTileForm`);
- `src/junction/placement/junction_placement.cpp`: `add_occupancy` and its item-row block (grep `recorded_item_rows`), `add_bend`, `row_pieces`, the W/E fan sizing (grep `port_rows`).

## Questions (answer each, with line numbers)
1. **Crosser tiles.** At the three seeds (T, LL_RR x L_R, L_R x LL_RR, built as the item-row tests in `src/tests/junction/topology/junction_topology.cpp` do), for every crosser occupant of an item row (kind "crosser" in `item_rows`), dump: edge, item row, the chain element that makes it (k, bend id, token, hin, hand, L), for a bend tile its index j and offset (dx, dy) and whether it lies in the bend's diagonal columns t = 1..T(L) of its column form or in a straight end; for a vertical tile, the two elements around it. Map each to its `JunctionArrangementUnder` candidate where the row is a head row of that crosser's Under; say what covers the crossers on item rows that are not head rows (if any). How many occupants does one (crosser, item row) have (a diagonal can put 2 tiles on one row)?
2. **Affine forms.** For each kind of cover (vertical run; bend diagonal column; bend straight end tile; Corner and S-bend; each heading and hand that occurs), write the crosser's west and east columns on item row y as `x[p] + c0 + c1 L + cy (y - y[p])` for a named point p, and the y conditions under which that cover is the one on row y (rows of the run; rows of the diagonal columns 1..T(L) from lo, hi, s; rows of an end tile). Use the column forms; do not invent constants. Check at the seeds: per hub, the number of crosser occupants whose recorded columns the form reproduces, and every mismatch.
3. **Exclusivity.** At the seeds, for every (crosser, item row), how many of the candidates' y conditions hold? A candidate set whose conditions overlap or leave gaps needs a tie rule: say which.
4. **Fans on item rows.** The skipped occupants (LL_RR x L_R 24): which fans (edge, side, port slot, port row, item row, hand, L, sigma = the sign of port row - item row as the recording sizes it), which item rows of which row they stand on, and their columns relative to that row's ranked occupants (all west of them? all east?). Which unrelated fan-fan pairs share a tile (the recon found 5) or touch?
5. **O2.** DESIGN-G4 O2 says adjacent fans of one side are separated at the rows of their four ends (port rows O + n, item rows), each fan's column there its diagonal line or its end column, the order fixed by sigma and the notation. At the seeds, list the fan pairs of each side that share rows, and check whether rows at those four ends alone separate every pair of their tiles (count tile pairs on one row that the four-end rows would not order). Which order does the notation give, and does sigma ever differ between two adjacent fans?
6. **Port stubs and runs.** Do the port tile and the run between a fan and its port stand on any other row's item row at the seeds? If so, how are their columns written?

## Do not
- Commit anything; leave `o4c2` clean (`git status --porcelain` empty apart from untracked files that were there) on `wt-recon-a6` at the launch head.
- Use `pkill`, a broad `kill`, or any form of `git stash`.
- Spend more than about 90 minutes; report what is done.

## Report (short)
- the recon file's path and a summary of six lines (one per question, in numbers);
- the probe patch's path;
- `git -C /home/pete/OpenTTD/.claude/o4c2 status --porcelain` and `git log -1`.

When you have reported, stop.
