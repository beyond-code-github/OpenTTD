# O4: a junction from the constraints (design, plain)

Principle (D152-D153): the code holds rules, costs and a search; no shapes. Layout = whatever rule-clean answer scores best.

## Inputs
`JunctionSpec` (notation, ml_gap, branch_gap, tl, design train -> prio_tiles, prices), terrain snapshot, anchor and orientation, a search box (grown a step when nothing fits).

## Given (not searched)
Still a place tool: the cursor and rotation fix these, nothing needs to exist yet (attach to an existing line is a later mode that only changes where they come from).
- The two main-line tracks: straight through the box at the cursor (rows 0 and ml_gap + 1), built by the tool as today; on terrain they are routed straight across the box by the same router (ramps, bridges, tunnels allowed).
- The branch: its two tracks leave the box through the S edge, branch_gap apart. Variable: the branch's position p along the edge (a few values around the anchor).

## Movements (a T)
EIn->SOut, WIn->SOut, NIn->EOut, NIn->WOut (today's port roles). Each one splits off a source track and merges into a target track. Its route is found by the one router (line router, one track; lane bundles later per notation):
- starts = every plain piece of the source track inside its split window;
- goals = every plain piece of the target track inside its merge window.
So where a movement leaves and joins is chosen by the router, inside rule windows, not by me.

## Windows = the rules, relative to what is already placed
Written as constraints between endpoints on the same track, from the rules the checker already enforces (D108 coverage, G9c no split inside a coverage, D148/D148a window start and phase, signal spacing, straight run before a merge, D113 no split in a bridge's block). Example: a merge into track T must lie at least prio_tiles + 2 (+ phase) past any split on T. The checker stays the final judge.

## Search: backtracking with incremental checks, branch and bound
1. Depth-first over the movements (and the branch position p, and the order where the window dependencies allow a choice). Each movement's alternatives, in order: the cheapest route in its window; the next window position; the route forbidden from the tiles a failing rule named; at a crossing, another crossing type (over, under, main line lifted). Not "k-best routes": they differ by irrelevant tiles.
2. After placing a movement, run the rule checks that only involve what is placed so far (per split and per merge: coverage, prio reach of that merge, no split inside a coverage, signal spacing on the stretch). Verify first which checker functions work on a partial layout (the signal pass assumes all ports); rules that don't run only on complete layouts. Each placed movement and its prio row are obstacles for the rest, at their own level only.
3. On a failure, chronological backtracking (previous movement's next alternative). Backjumping to the movement a rule names only if measurements show plain backtracking is too slow.
4. Complete layouts: full checker and prio timing once; score compactness (occupied + slivers, D153), then cost. Branch and bound on occupied tiles only (the one part that only grows; slivers don't).
5. Known limit: a choice is revisited only on failure or through the bound, so an early movement may crowd the others into a worse layout. If measurements show it, also try each movement's few real alternatives without a failure, or other orders.
6. Per rule, count rejections; a rule that rejects often moves earlier (into the windows or the router) so it prunes sooner (as D148a did).

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

## Crossing a main line (three ways, cost decides)
1. The movement bridges over the main line (line router bridges, LP6, D135 d).
2. The movement tunnels under it (O2).
3. The main line goes over the movement on a doubled bridge (D80), as today: while a movement is routed, main-line tiles are crossable at ground level for the main line's share of a bridge (today's crossable_cost); after all movements are placed, each crossed main-line stretch is lifted onto a doubled bridge spanning those tiles, then the checker judges it (bridge length, D80 doubling, prio coverage across the bridge). If it breaks a rule, that crossing is retried as 1 or 2.
Open for the engineer: penalise lifting the main line beyond money (one long block, a slope at line speed), or money alone.
