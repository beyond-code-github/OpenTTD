# O4: a junction from the constraints (design, plain)

Principle (D152-D153): the code holds rules, costs and a search; no shapes. Layout = whatever rule-clean answer scores best.

## Inputs
`JunctionSpec` (notation, ml_gap, branch_gap, tl, design train -> prio_tiles, prices), terrain snapshot, anchor and orientation, a search box (grown a step when nothing fits).

## Given (not searched)
- The two main-line tracks: straight through the box (rows 0 and ml_gap + 1, as the lines being joined).
- The branch: its two tracks leave the box through the S edge, branch_gap apart. Variable: the branch's position p along the edge (a few values around the anchor).

## Movements (a T)
EIn->SOut, WIn->SOut, NIn->EOut, NIn->WOut (today's port roles). Each one splits off a source track and merges into a target track. Its route is found by the one router (line router, one track; lane bundles later per notation):
- starts = every plain piece of the source track inside its split window;
- goals = every plain piece of the target track inside its merge window.
So where a movement leaves and joins is chosen by the router, inside rule windows, not by me.

## Windows = the rules, relative to what is already placed
Written as constraints between endpoints on the same track, from the rules the checker already enforces (D108 coverage, G9c no split inside a coverage, D148/D148a window start and phase, signal spacing, straight run before a merge, D113 no split in a bridge's block). Example: a merge into track T must lie at least prio_tiles + 2 (+ phase) past any split on T. The checker stays the final judge.

## Search
1. For each branch position p, and each order of the four movements compatible with the window dependencies (a merge's window needs the splits on its track first; a handful of orders):
2. Route the movements one at a time; each routed movement, and the prio coverage row beside its merge (the tiles the signal pass will add), become obstacles for the next (at their level only: passing under or over is a priced move).
3. Signal pass, checker. On a rule failure, retry that movement with its window start moved one tile (the phase case), at most a few times; then the next order.
4. Score: compactness (occupied + slivers, D153), then cost. Keep the best.

Expected work: 4 routes x a few orders x a few p x few retries, each route a few ms: tens of ms. O5 measures it against 100 ms.

## Steps
| Step | What |
|---|---|
| O4a | Line router takes a set of start pieces (`starts`) as well as one start (root per start). Tests: the cheaper of two starts is used; one start equals today. |
| O4b | `GenerateJunctionOrganic` for the T on flat ground with the search above. Outer tests: rule-clean on MakeTestJunctionSpec and PinnedTestSpec; the Pinned TL6 branch_gap 0 case builds; compactness reported beside today's generator (193 tiles / 28% slivers). |
| O4c | On terrain (after O2/O3): rule-clean on a hill, across a river, under a ridge. |
| O4d | Behind `GenerateJunction`/`GenerateJunctionNear` (GUI, library, rig) in place of the template; old path deleted. |

## Reused
Line router (O1, O1b, O2), checker, signal pass and prio placement, prio timing/sizing, pricing, build sequencer, compactness measure (C1), ghost.
