# L1: occupancy legality from the graph, not from a reference solve (REVIEW-opus-since-terrain-placement top change 1; D193, D217)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git checkout -B wt-l1 <HEAD>` (given at launch; the worktree is on wt-r7 with a WIP commit: leave that branch alone). Never use git stash. Another agent may work in .claude/nodes2: never touch it.
- The review (docs/junction-tool/REVIEW-opus-since-terrain-placement.md, read its occupancy sections) found: the occupancy family's legality is whitelisted by EDGE PAIR from the state's own flat reference solve (junction_topology.cpp ~:1999, ~:2008): one shared tile between two edges makes every tile they share legal; node and prio occupants carry one edge id, so one node sharing legalises all; runs empty at the flat solve are not occupants (~:1902). The whitelist came from T1c-2 Amendment 2, motivated by e12/e17 sharing an SE track end at (28,11) on LL_RR's seed while the checker accepts it.

## Step 0 (findings first, no code change)
Explain e12/e17 at (28,11) and (28,12) from the code: which graph elements those pieces belong to (tags, tokens), whether the graph relates them (a node, a bridge, a shared lane), and how the checker's tracing accepts a shared track end there (file:line). Write it to the report. If the honest answer is "the graph relates them" — say through what — that relation is what the rule must read. If they are unrelated and connect, report it as a checker gap and STOP before changing code.

## Changes (after step 0)
1. Legality per occupant pair, from the graph and the bits only: two occupants on one tile are legal when the graph relates their pieces (same edge and copy; a node tile with an edge that starts or ends at that node; the relation step 0 names, if any) or when their pieces share no track end on any tile edge. Remove the reference-solve whitelist (`ref_shares` and its edge-pair set).
2. Runs that are empty at the flat solve still enter as occupants (zero-length runs become single-tile or empty intervals: say how).
3. Node and prio occupants are related only to the edges that touch that node (not "any edge sharing an id").

## Tests (record each red)
1. **Outer:** every existing test and sweep green with identical numbers (seeds, sweeps, circuit byte-identical). If a seed now fails the occupancy legality, report the tile, edges and bits: STOP (a real collision the whitelist hid, or a missing graph relation).
2. **`[topology]` "topology: occupancy legality reads the graph"**: (a) two unrelated edges forced onto one tile with a shared track end (hand-made via x_floors on the T) fail with "occupancy"; (b) the same two edges sharing a tile without a shared track end are legal; (c) a node tile shared with an edge that does not touch that node fails. Red: the whitelist makes (a) or (c) legal when a reference share exists (construct the case so it does).

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `'[topoproj]' 5`; `build-rel/openttd_test` with `"[topologysweep]"`, `"[topologystretch]"`, `"[placeoracle]"`, `"[realterrainsweep]"`, `"[topoprojsweep]"` (diff against launch HEAD); circuit byte-identical; `tools/junction/jt gate build-rel` (report the count).
- One commit: `Fix: Junction occupancy legality from the graph, not from a reference solve`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus step 0's explanation and any counts that moved. When you have reported, stop.
