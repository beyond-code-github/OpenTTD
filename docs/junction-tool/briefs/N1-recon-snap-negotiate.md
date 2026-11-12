# N1 (recon by experiment): how big is snap-and-negotiate on real terrain (DESIGN-G1 §17)

Recon by experiment: you may change code in your worktree to measure, but commit nothing and leave the worktree clean (`git checkout -- src`, delete scratch files; save the experiment as a patch in the scratchpad). Keep your context small: send command output to files and report only what matters. Never use git stash. Mark anything not measured or read as INFERRED.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes2 (Debug `build`, Release `build-rel`). First: `git checkout -B wt2-n1 <HEAD>` (given at launch). Another agent works in .claude/nodes: never touch it.
- DESIGN-G1 §17 (read it whole): when the hover plan fails, failing nodes get candidate spots along their row (scored by the node tile's own cost) and failing edges get traced shape alternatives (no shift, Shift pair either side, S-bend m +/- 1) filtered by hard facts only, costed with a lower-bound terraform estimate; a best-first assignment picks one candidate per item, pre-checked by tile overlap, confirmed by the full solve. Before building it, we need its sizes and times.
- The hover plan today: PlanJunctionHover (src/junction_hover.cpp) plans 19 of 48 real fixtures (`[realterrainsweep]`); failures carry a reason chain (`placement.why`, JunctionPlacementReason with rule, edge, node, row; the fact's tile in the error text).

## Read first (once each, offset/limit)
- DESIGN-G1.md §14.4, §15, §16, §17; src/junction_hover.cpp; src/junction_topology.cpp the facts block and reasons, the Elem chains, edge_tiles; src/junction_levels.cpp the per-tile cost table; the R7 Shift edit if merged (ApplyJunctionShifts), otherwise trace Shift shapes geometrically.

## Experiments (Release, on the 29 fixtures the hover plan fails today, T topology)
1. Per fixture: the failure's first reason; classify it as NODE (a node tile or its element), EDGE (a run or bend tile of edge e), or OTHER (rule name). How many distinct failing items appear if you iterate: block the item's current choice and re-solve (up to 5 rounds) — the number of items a negotiation would have to handle.
2. Node spots: for each failing node, its legal column range (Lo..Up of its point in the failing solve's propagation before the failure, or the flat solve's range), and how many columns have a hard-fact-free node tile; the cost distribution of the best 3.
3. Edge shapes: for each failing edge, trace its shape alternatives between its current ends (tiles from the chain machinery for each alternative token sequence: compute the tiles as edge_tiles would after a program change; geometry only, no full solve): how many survive the hard facts; trace time per edge (microseconds).
4. Negotiation, simplest form: for each fixture, try the assignments in best-first order (cheapest candidates first) with the FULL solve (placement + levels + render + signals) until one passes or 20 full solves: how many fixtures become plannable, how many full solves each needed, the wall time per fixture (mean/max), and which assignments failed and why (first reason).
5. What the remaining failures need (row gap, node move to another row, tunnel...): grouped.

## Output
docs/junction-tool/RECON-snap-negotiate.md (untracked, ~80 lines) and the patch path.

## Report
The conclusions in 10 lines: failing items per fixture, candidates per item, trace time, fixtures plannable by negotiation (vs 19 today), full solves and wall time per hover, what remains. `git status --porcelain` (clean), `git log -1`. When you have reported, stop.
