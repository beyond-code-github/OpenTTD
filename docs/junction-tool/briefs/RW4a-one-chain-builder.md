# RW4a: one token-driven chain builder for every row edge (DESIGN-G1 §16.4 RW4, first part; RECON-per-row-code.md R5-R7)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes2 (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/nodes2 checkout -B wt2-rw4a <HEAD>` (given at launch). Never use git stash. Another agent works in /home/pete/OpenTTD/.claude/nodes (a small change to the facts overload of PlaceJunctionTrackGraph): never touch that worktree. Use absolute paths.
- RW1-RW3 merged (row items from tokens, S-bends sized by rows on any edge, edge roles from predicates JunctionIsInEdge/OutEdge/LegEdge/DoubledEdge). Remaining per-row code (RECON-per-row-code.md §1): R5 the chain build splits three ways (in/out: Fan + SBends only, anything else fails; undoubled leg: two Tiles; doubled leg: Tile, HeadsPair.., SBend, Tile + copy chain), R6 row_changes by role, R7 row_pieces(r) walks in edge, a hand-built leg list, out edge. R4 row_pts allocates per-row point blocks so point ids stay.
- This part keeps row_pts (point ids unchanged); RW4b removes it later.

## Read first (once each)
CODINGSTYLE.md (C++ part); RECON-per-row-code.md §1, §4; DESIGN-G1 §16.1-§16.4; src/junction_topology.cpp the chain build (R5), row_changes (R6), row_pieces (R7) and the Elem struct/kinds.

## Step 0 (findings, no code change)
For each of R5's three branches, R6 and R7: what it produces per token (Elem kind, index, points it takes from row_pts), and whether a single loop over the edge's program tokens in order (Fan, SBend, HeadsPair/Over, role-tagged SBends, Tile at nodes) can produce exactly the same Elems and points. Name any output that depends on the edge's role rather than its tokens (file:line). If such a dependency encodes a rule not expressible from the tokens (e.g. a Tile with no token), report it and stop.

## Changes (after step 0; src/junction_topology.cpp)
1. One builder: a single loop over each row edge's (and copy's) tokens that emits Elems by token kind, taking points from row_pts exactly as today, replacing R5's three branches. Roles enter only through tokens and the RW3 predicates where step 0 shows a token alone cannot decide (each such use commented with why).
2. row_changes (R6) and row_pieces (R7) read the chains the builder made, not per-role code: row_pieces(r) = the pieces of the chains of the row's edges in travel order.
3. Comments move with the code.

## Tests
1. **Outer (identity):** build-rel outputs of `"[topologysweep]"` (incl. EDITSOUND), `"[topologystretch]"`, `"[placeoracle]"`, `"[realterrainsweep]"`, `"[topoprojsweep]"` equal to launch HEAD's with times stripped; the LL_RR circuit regenerated with `[.circuitllrr]` and `JUNCTION_CIRCUIT` as earlier steps did, compared with `cmp` (byte-identical); `[topology]`, `[topoproj]` unchanged.
2. A temporary assert during development that the new builder's Elems equal the old branches' Elems on every placement in the sweeps (run, then remove both the old code and the assert). Report that it held.

## Stop conditions
- Step 0 finds a role-only rule; any identity difference; anything not covered.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `'[topoproj]' 5`; the identity diffs; `tools/junction/jt gate build-rel` (count, time).
- One commit: `Codechange: Junction row edges built by one token-driven chain builder`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus step 0's table, what still reads roles (and why), and the identity result. When you have reported, stop.
