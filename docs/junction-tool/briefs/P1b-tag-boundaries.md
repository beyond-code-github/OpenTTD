# P1b: route tags end each edge at its node, and copy names the program (DESIGN-G1 §13.2)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git checkout -B wt-p1b <HEAD>` (given at launch). Never use git stash.
- P1 (merged) tags every route piece (JunctionTrackPieceTag: edge, token or RUN, run, copy) in RenderJunctionTrackPlacement's route loop. Two of its choices are wrong for what the tags are for (failures and rules name the run of an edge, e.g. D212's port edge run between the Fan and the exit split):
  1. A run of pieces after an edge's last emitting token is tagged with the NEXT edge. Pieces belong to the edge they lie on: the boundary between consecutive edges e1 -> e2 of a route is their shared node n, at the route's first non-bridge piece on placement.nodes[n] (the node piece: a split's first diverging half piece, a merge's last joining half piece). The node piece of a MERGE and every piece before it belong to e1; the node piece of a SPLIT and every piece after it belong to e2.
  2. `copy` is the route's copy even on edges that are not doubled. It must be 1 only on a doubled edge (one with a program in JunctionTrackPrograms::copies) on a copy-1 route, so that `token` indexes programs.copies[edge] when copy is 1 and programs.edges[edge] otherwise (as the struct's comment says).
- `run` stays "the number of the edge's tokens before this piece" counted within the piece's own edge.

## Read first
- CODINGSTYLE.md (C++ part); src/junction_topology.h JunctionTrackPieceTag, JunctionTrackRoute, JunctionTrackPlacement; junction_topology.cpp RenderJunctionTrackPlacement's route loop (the tag code P1 added; `doubled_leg`); src/tests/junction_topology.cpp "route pieces carry their graph tags"; junction_nodes.cpp JunctionNodeInputOfGraph (how it uses tags for bridges).

## Tests (tag `[topology]`; record each red)
1. **Edit "topology: route pieces carry their graph tags"** (allowed edit) to add, for the T and LL_RR: along each route the tag edges form contiguous blocks in the order of the lane path's edges (graph.path_edges); at each node between consecutive edges the boundary is as State item 1 says; `copy == 1` only where the edge has a copies program and the route is copy 1; every tag's token (when not RUN) is a valid index into the program its copy names, and its run equals the number of that program's tokens before it. Red: the boundary or copy checks fail on the current tags (record which).
2. "topology: the graph builds the traced template" stays green unedited (tags do not change the input otherwise).

## Changes
- In the route loop: assign each piece's edge by State item 1 (you may assign edges in a pass over the finished route using placement.nodes and the lane path's edges, then recompute `run` per edge from the token tags; keep token tags as they are). Set copy by State item 2.
- If JunctionNodeInputOfGraph's bridge split relies on the old edge tags, keep its result identical (test 2).

## Do not
- Do not change pieces, tokens of bend/bridge pieces, the render's layout, or any other function.

## Stop conditions
- A route whose node piece is missing, or blocks out of order; an existing test fails; anything not covered.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `tools/junction/jt gate build-rel` (393 before).
- One commit: `Fix: Junction route tags end each edge at its node; copy names the program`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report. When you have reported, stop.
