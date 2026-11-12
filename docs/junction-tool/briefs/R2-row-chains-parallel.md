# R2: row edges as Elem chains, built beside row_pts and checked equal (DESIGN-G1 §16 R2; RECON-row-chain §4 steps 2)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git checkout -B wt-r2 <HEAD>` (given at launch). Never use git stash. Another agent works in .claude/nodes2: never touch it.
- R1 merged: run floors come from JunctionRunFloor(FloorKind, FloorKind, ...). DESIGN-G1 §16: every edge becomes one Elem chain; this step builds the row edges' chains IN PARALLEL with row_pts, adds no relations of their own beyond tying each chain point to its row_pts counterpart, and checks that the chain machinery reproduces the row geometry. Nothing else changes.
- RECON-row-chain.md §4 (read it whole) gives the new Elem kinds: Fan, horizontal SBend sized by rows, the lane split/merge copy pair (two chains per doubled edge), HeadsPair.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); TESTS.md; DESIGN-G1.md §16; RECON-row-chain.md whole.
- src/junction_topology.cpp the placement helper: Elem and its kinds, chain building for non-row edges, RowPts and the row blocks, edge_tiles, sym routes, the D215 objective's RowChange list (it keys on point ids today).

## Tests (record each red)
1. **Outer `[topology]` "topology: row chains reproduce the row geometry"**: for T and LL_RR at the test spec and every `[topologysweep]` cell (put the sweep part in `[topologysweep]`): for every row edge (in, leg, out; both copies of a doubled leg), the row chain's elements, their kinds in order, and their solved points equal the row_pts geometry (fans: first/last; lane split/merge; heads per bridge and copy; nodes), and `edge_tiles` of the row chain equals the tiles the row blocks enumerate today for that edge (the facts/occupancy enumeration). Red: no row chains (record).
2. **`[topology]` "topology: the D215 objective keys on graph elements"**: the RowChange list's entries carry (edge, token, copy) and the objective's result is unchanged (SamePlacement on T and LL_RR); building the point ids in a different order (a test-only reversal is NOT required; instead assert the list is sorted by (edge, copy, token) and contains no point-id ordering dependence by reading) — keep it simple: assert the keys and SamePlacement.
- Every existing test unchanged and green; circuit byte-identical; sweeps green with identical numbers.

## Changes
1. Elem gains the kinds the recon names that rows need now: `Fan` (size from port row; m 0 allowed: first == last), and the doubled leg pair as two chains per doubled edge (copy 0: Tile.., Heads.., SBend; copy 1: SBend, Heads..) with `HeadsPair` for row bridge heads (copy 1 = copy 0 + u). Horizontal SBend sized by rows. Add only what row edges at T and LL_RR need; name each kind's comment with its rule.
2. Build `chain[e]` (and a copies chain) for every row edge from the programs, as for other edges; for each chain point add `eq` to its row_pts counterpart in both phases (reason "row chain (R2)") so the solve is unchanged.
3. The D215 objective's RowChange entries gain their (edge, token, copy) key and are ordered by it, not by point ids.
4. TESTS.md rows.

## Do not
- No relation from the chain machinery on row chains beyond the `eq` ties (no floors, no families over them yet: those switch in R3-R5). No removal of row_pts. No program or render change.

## Stop conditions
- A row feature with no Elem kind in the recon's list; any seed or sweep number changes; anything not covered.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `tools/junction/jt fast build '[topoproj]' 5`; `build-rel/openttd_test` with `"[topologysweep]"`, `"[topologystretch]"`, `"[placeoracle]"`, `"[realterrainsweep]"`, `"[topoprojsweep]"` (to files); circuit byte-identical; `tools/junction/jt gate build-rel` (444 before; report the count).
- One commit: `Codechange: Junction row edges as Elem chains beside the row points (R2)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus the new Elem kinds, the per-edge chain kinds for LL_RR row 0 and a doubled row, and timing changes. When you have reported, stop.
